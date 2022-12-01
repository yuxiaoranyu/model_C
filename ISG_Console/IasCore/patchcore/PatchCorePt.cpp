#include "PatchCorePt.h"
#include <time.h>

PatchCorePt::PatchCorePt(Config_Data &config, const int batch_size, const bool isGPU)
{
    (void)batch_size; //不支持batch推理，如果推理时，输入多张图，仅对第一张图进行检测
    (void)isGPU;      //仅支持GPU方式

    std::cout << "patchcore init: " << config.model_path << std::endl;
    std::cout << "patchcore cuda: " << torch::cuda::is_available() << std::endl; //检查GPU是否可用
    m_config_data = config;                                                      //存储算法参数

    m_model = torch::jit::load(config.model_path); //python深度学习框架，加载模型，返回模型对象
    m_model.eval();                                //不启用 Batch Normalization 和 Dropout

    //算法初始被调用，推理开销长。预触发模型几次forward()，降低算法首次处理的时间开销
    torch::Tensor warm_up_tensor = torch::ones({1, 3, 224, 224}).to(torch::kCUDA).to(torch::kHalf); //在cuda定义一个内容为全1的3通道的 224 * 224的张量
    std::vector<torch::jit::IValue> warm_up_input;
    warm_up_input.push_back(warm_up_tensor);
    for (int w = 0; w < 5; w++)
    {
        auto warm_up_infer = m_model.forward(warm_up_input);
    }

    //读取模板文件
    if (m_config_data.btemplate_match)
    {
        std::vector<std::string> templateImage_filename;
        cv::glob(config.template_path, templateImage_filename, false); //遍历template_path路径下的所有模板图像文件，不遍历子文件夹
        for (size_t i = 0; i < templateImage_filename.size(); i++)
        {
            m_template_mat_vector.push_back(cv::imread(templateImage_filename[i]));
        }
    }
}

bool PatchCorePt::update(Config_Data &config)
{
    //清除历史配置数据
    m_template_mat_vector.clear();

    //重新加载配置
    m_config_data = config;

    //读取模板文件
    if (m_config_data.btemplate_match)
    {
        std::vector<std::string> templateImage_filename;
        cv::glob(config.template_path, templateImage_filename, false); //遍历template_path路径下的所有模板图像文件，不遍历子文件夹
        for (size_t i = 0; i < templateImage_filename.size(); i++)
        {
            m_template_mat_vector.push_back(cv::imread(templateImage_filename[i]));
        }
    }

    return true;
}

PatchCorePt::~PatchCorePt()
{
}

PatchCore_Forward_Result PatchCorePt::forward(const std::vector<cv::Mat> &images)
{
    cv::Mat read_image = images[0];
    cv::cvtColor(read_image, read_image, cv::COLOR_BGR2RGB);                        //将图像转换成RGB格式
    cv::resize(read_image, read_image, cv::Size(224, 224), 0, 0, cv::INTER_LINEAR); //图片缩放为224*224
    read_image.convertTo(read_image, CV_32FC3, 1.0 / 255.0);                        //图像转换，转换格式成32位3通道浮点型图片，把原矩阵中的每一个元素都乘以1/255

    //张量转换
    torch::Tensor tensor_image = torch::from_blob(read_image.data, {1, read_image.rows, read_image.cols, 3});
    tensor_image = tensor_image.to(torch::kCUDA);                    //张量存储到cuda
    tensor_image = tensor_image.to(torch::kHalf);                    //数据类型设置成float16
    tensor_image = tensor_image.permute({0, 3, 1, 2});               //张量换位
    tensor_image[0][0] = tensor_image[0][0].sub_(0.485).div_(0.229); // 0通道数据变换
    tensor_image[0][1] = tensor_image[0][1].sub_(0.456).div_(0.224); // 1通道数据变换
    tensor_image[0][2] = tensor_image[0][2].sub_(0.406).div_(0.225); // 2通道数据变换

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor_image);
    auto output = m_model.forward(inputs).toTuple(); //张量数据处理

    torch::Tensor anomaly_map_T = output->elements()[0].toTensor(); //异物检测结果转换成张量
    int width = anomaly_map_T.sizes()[0];
    int height = anomaly_map_T.sizes()[1];
    anomaly_map_T = anomaly_map_T.mul(255).clamp(0, 255).to(torch::kU8);
    anomaly_map_T = anomaly_map_T.to(torch::kCPU); //张量取回CPU

    cv::Mat output_mat(height, width, CV_8UC1, anomaly_map_T.data_ptr()); //输出热力灰度图
    torch::jit::IValue pred_score = output->elements()[1];                //输出得分

    PatchCore_Forward_Result forward_result;
    forward_result.score = pred_score.toTensor().item<float>();
    forward_result.output_mat = output_mat;
//    std::cout<<"infer score ******* "<<forward_result.score<<std::endl;
    return forward_result;
}

PatchCore_Result PatchCorePt::infer(const std::vector<cv::Mat> &images)
{
    PatchCore_Result patchCoreResult;

    //调用PatchCore推理
    PatchCore_Forward_Result forward_result = forward(images);

    //算法得分输出范围为0~1；得分越接近0，越正确，得分越接近1，越有可能为错误;
    patchCoreResult.patchcore_score = forward_result.score;

    //后处理
    post_process(images[0], forward_result.output_mat, patchCoreResult);

    return patchCoreResult;
}

void PatchCorePt::post_process(const cv::Mat &oriImg, const cv::Mat &heat_map, PatchCore_Result &patchCoreResult)
{
    // patchcore算法推理得分小于最小阈值，无缺陷
    if (patchCoreResult.patchcore_score < m_config_data.score_min_threshold)
    {
        patchCoreResult.result = true;
    }
    else if ((patchCoreResult.patchcore_score >= m_config_data.score_min_threshold) && (patchCoreResult.patchcore_score <= m_config_data.score_max_threshold))
    {
        //无缺陷
        if (!m_config_data.btemplate_match)
        {
            patchCoreResult.result = true;
            return;
        }
        // 缺陷区域查找，并进行模板匹配
        bool bFindDefectArea = findDefectArea(oriImg, heat_map, patchCoreResult);
        if (bFindDefectArea)
        {
            //如果未配置有效区域或点在有效区域内，认为找到区域
            if (matchTemplate(oriImg, patchCoreResult))
            {
                patchCoreResult.result = true; //认为无缺陷
            }
            else
            {
                patchCoreResult.result = false; //认为缺陷
            }
        }
        else
        {
            patchCoreResult.result = true; //未发现有效区域，认为无缺陷
        }
    }
    else
    {
        patchCoreResult.result = false; //得分大于最大阈值，认为缺陷
        findDefectArea(oriImg, heat_map, patchCoreResult); //缺陷区域查找
    }
}

bool PatchCorePt::findDefectArea(const cv::Mat &oriImg, const cv::Mat &heat_map, PatchCore_Result &patchCoreResult)
{
    double Min_heat, Max_heat;                     //热力图中的最小值和最大值
    cv::minMaxLoc(heat_map, &Min_heat, &Max_heat); //获取图的热力最小值和热力最大值
    int histsize = Max_heat + 1;
    float ranges[] = {0, (float)Max_heat + 1};
    const float *histRanges = {ranges};
    cv::Mat hist;                                                              //记录热力灰度分布数组
    cv::calcHist(&heat_map, 1, 0, cv::Mat(), hist, 1, &histsize, &histRanges); //创建灰度分布直方图

    int max_step = 21; //只取最大的21个灰度
    float num = 0;
    int value_threshold = m_config_data.heat_threshold; //热力图二值化的阈值
    int value_tmp = 0;
    for (int i = 0; i < max_step; i++)
    {
        num += hist.at<float>(Max_heat - i);
        if (num > m_config_data.heat_binary_min_area)
        {
            value_tmp = Max_heat - i;
            if (value_tmp > value_threshold)
            {
                value_threshold = value_tmp; //取出灰度大于150，且对应灰度的累积点数超过100的灰度
            }
            break;
        }
    }
    cv::Mat heat_resize;
    cv::resize(heat_map, heat_resize, cv::Size(oriImg.cols, oriImg.rows)); //热力图缩放，与原始图相同大小

    cv::Mat binaryMat;
    cv::threshold(heat_resize, binaryMat, value_threshold - 1, 255, cv::THRESH_BINARY); //二值化热力图

    // 寻找疑似区域
    std::vector<std::vector<cv::Point>> maybe_contours; //疑似区域
    std::vector<cv::Vec4i> hierachy;
    cv::findContours(binaryMat, maybe_contours, hierachy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE, cv::Point(0, 0)); //从二值图像中提取轮廓
    if (maybe_contours.size() != 0)
    {
        std::vector<cv::Rect> maybe_rois; //疑似区域矩形
        for (size_t i = 0; i < maybe_contours.size(); ++i)
        {
            cv::Rect maybe_roi_rect = cv::boundingRect(cv::Mat(maybe_contours[i]));
            maybe_rois.push_back(maybe_roi_rect);
        }

        //找出最大的框
        if (maybe_rois.size() > 0)
        {
            int max_area = 0;
            int n = 0;
            for (size_t i = 0; i < maybe_rois.size(); i++)
            {
                if (maybe_rois[i].area() > max_area)
                {
                    max_area = maybe_rois[i].area();
                    n = i;
                }
            }
            if (max_area >= m_config_data.defective_min_area) //缺陷区域pixel最小阈值
            {
                patchCoreResult.ltx = std::max(maybe_rois[n].x, 0);
                patchCoreResult.lty = std::max(maybe_rois[n].y, 0);
                patchCoreResult.rbx = std::min(maybe_rois[n].x + maybe_rois[n].width, oriImg.cols);
                patchCoreResult.rby = std::min(maybe_rois[n].y + maybe_rois[n].height, oriImg.rows);
            }
        }
    }

    int area_size = (patchCoreResult.rbx - patchCoreResult.ltx) * (patchCoreResult.rby - patchCoreResult.lty);
    if ((area_size == 0) || ((value_threshold == 255) && (area_size > 10000)))
    {
        return false;
    }

    cv::Point center((patchCoreResult.ltx + patchCoreResult.rbx) / 2, (patchCoreResult.rbx + patchCoreResult.rby) / 2);

    if ((m_config_data.effective_area.size() == 0) || ((m_config_data.effective_area.size() > 0) && isInPolygon(center, m_config_data.effective_area)))
    {
        //如果未配置有效区域或点在有效区域内，认为找到区域
        return true;
    }
    else
    {
        return false;
    }
}

bool PatchCorePt::matchTemplate(const cv::Mat &oriImg, PatchCore_Result &patchCoreResult)
{
    if (m_template_mat_vector.size() == 0)
    {
        return true;
    }

    // ***模板匹配判断***
    int area_size = (patchCoreResult.rbx - patchCoreResult.ltx) * (patchCoreResult.rby - patchCoreResult.lty);
    if (area_size > 3600)
    {
        //大区域缩减到3600
        int width = patchCoreResult.rbx - patchCoreResult.ltx;
        int height = patchCoreResult.rby - patchCoreResult.lty;
        int center_point_x = patchCoreResult.ltx + width / 2;
        int center_point_y = patchCoreResult.lty + height / 2;

        while (width * height > 3600)
        {
            if (width > 60)
            {
                width = width - 3;
            }
            if (height > 60)
            {
                height = height - 3;
            }
        }

        patchCoreResult.ltx = std::max(center_point_x - width / 2, 0);
        patchCoreResult.lty = std::max(center_point_y - height / 2, 0);
        patchCoreResult.rbx = std::min(center_point_x + width / 2, oriImg.cols);
        patchCoreResult.rby = std::min(center_point_y + height / 2, oriImg.rows);
    }

    cv::Rect rect(cv::Point(patchCoreResult.ltx, patchCoreResult.lty), cv::Point(patchCoreResult.rbx, patchCoreResult.rby)); // patchcore计算出的区域
    cv::Rect dst_rect(cv::Point(std::max(rect.x - m_config_data.expand_pixel, 0),
                                std::max(rect.y - m_config_data.expand_pixel, 0)),
                      cv::Point(std::min(rect.br().x + m_config_data.expand_pixel, oriImg.cols),
                                std::min(rect.br().y + m_config_data.expand_pixel, oriImg.rows))); //膨胀后的区域
    cv::Mat src_gray;
    if (oriImg.channels() == 1)
    {
        src_gray = oriImg;
    }
    else
    {
        cv::cvtColor(oriImg, src_gray, cv::COLOR_BGR2GRAY);
    }

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    for (size_t i = 0; i < m_template_mat_vector.size(); ++i)
    {
        cv::Mat image_matched; //模板匹配结果
        cv::Mat template_imgRoi = m_template_mat_vector[i](dst_rect);
        cv::cvtColor(template_imgRoi, template_imgRoi, cv::COLOR_BGR2GRAY);
        //归一化
        cv::matchTemplate(template_imgRoi, src_gray(rect), image_matched, cv::TM_CCOEFF_NORMED); //模板匹配
        cv::minMaxLoc(image_matched, &minVal, &maxVal, &minLoc, &maxLoc);                        //寻找最佳的匹配位置
        cv::Rect matched_rect(maxLoc.x, maxLoc.y, rect.width, rect.height);                      //匹配区域
        cv::Mat template_rect_binary = template_imgRoi(matched_rect);                            //匹配区域的模板图数据
        cv::Mat srcgray_rect_binary = src_gray(rect);                                            //匹配区域的待检测图数据
        cv::Mat diff_img, module_binary;

        cv::absdiff(template_rect_binary, srcgray_rect_binary, diff_img); //计算两个数组差的绝对值
        cv::threshold(diff_img, module_binary, m_config_data.diff_threshold - 1, 255, cv::THRESH_BINARY);
        cv::Scalar sum_value = cv::sum(module_binary);
        int point_count = sum_value[0] / 255; //原图和模板图做差后二值化的点数阈值
        if (point_count <= m_config_data.dismatched_point_threshold)
        {
            patchCoreResult.ltx = 0;
            patchCoreResult.lty = 0;
            patchCoreResult.rbx = 0;
            patchCoreResult.rby = 0;
            return true;
        }
    }

    return false;
}

bool PatchCorePt::isInPolygon(const cv::Point &point, const std::vector<cv::Point> &points)
{
    int intersection_num = 0;
    int cross_num = 0;
    for (size_t i = 0; i < points.size(); i++)
    {
        // base on ray method
        cv::Point p1, p2;
        if (i < points.size() - 1)
        {
            p1 = points.at(i);
            p2 = points.at(i + 1);
        }
        else
        {
            p1 = points.at(i);
            p2 = points.at(0);
        }

        if (p1.y > point.y && p2.y > point.y)
        {
            continue;
        }

        if (p1.y < point.y && p2.y < point.y)
        {
            continue;
        }

        if (p1.y == point.y)
        {
            cross_num++;
        }

        float x;

        if (p1.y == p2.y)
        {
            x = std::min(p1.x, p2.x);
        }
        else
        {
            x = ((p1.x - p2.x) / (p1.y - p2.y)) * (point.y - p1.y) + p1.x;
        }

        // calculate all right intersection
        if (x > point.x)
        {
            intersection_num++;
        }
    }

    if ((intersection_num - cross_num) % 2 == 1)
        return true;
    else
        return false;
}
