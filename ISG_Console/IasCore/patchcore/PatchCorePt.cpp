#include "PatchCorePt.h"
#include <time.h>

PatchCorePt::PatchCorePt(Config_Data &config, const int batch_size, const bool isGPU)
{
    (void)batch_size; //不支持batch推理，如果推理时，输入多张图，仅对第一张图进行检测
    (void)isGPU; //仅支持GPU方式

    std::cout << "patchcore::init" << config.model_path << std::endl;
    std::cout << "cuda" << torch::cuda::is_available() << std::endl;  //检查GPU是否可用
    m_config_data = config; //存储算法参数

    m_model = torch::jit::load(config.model_path); //python深度学习框架，加载模型，返回模型对象
    m_model.eval();  //不启用 Batch Normalization 和 Dropout

    //算法初始被调用，推理开销长。预触发模型几次forward()，降低算法首次处理的时间开销
    torch::Tensor warm_up_tensor = torch::ones({1, 3, 224, 224}).to(torch::kCUDA).to(torch::kHalf); //在cuda定义一个内容为全1的3通道的 224 * 224的张量
    std::vector<torch::jit::IValue> warm_up_input;
    warm_up_input.push_back(warm_up_tensor);
    for (int w = 0; w < 5; w++)
    {
        auto warm_up_infer = m_model.forward(warm_up_input);
    }

    //读取模板文件
    std::vector<std::string> templateImage_filename;
    cv::glob(config.template_path, templateImage_filename, false); //遍历template_path路径下的所有模板图像文件，不遍历子文件夹
    for (size_t i = 0; i < templateImage_filename.size(); i++)
    {
        m_template_mat_vector.push_back(cv::imread(templateImage_filename[i]));
    }
}

PatchCorePt::~PatchCorePt()
{
}

RoiInfo PatchCorePt::infer(const std::vector<cv::Mat> &images)
{
    RoiInfo roiInfo;

    auto start_time = std::chrono::system_clock::now();

    cv::Mat read_image = images[0];
    cv::cvtColor(read_image, read_image, cv::COLOR_BGR2RGB); //将图像转换成RGB格式
    cv::resize(read_image, read_image, cv::Size(224, 224), 0, 0, cv::INTER_LINEAR); //图片缩放为224*224
    read_image.convertTo(read_image, CV_32FC3, 1.0 / 255.0); //图像转换，转换格式成32位3通道浮点型图片，把原矩阵中的每一个元素都乘以1/255

    //张量转换
    torch::Tensor tensor_image = torch::from_blob(read_image.data, {1, read_image.rows, read_image.cols, 3});
    tensor_image = tensor_image.to(torch::kCUDA);//张量存储到cuda
    tensor_image = tensor_image.to(torch::kHalf);//数据类型设置成float16
    tensor_image = tensor_image.permute({0, 3, 1, 2}); //张量换位
    tensor_image[0][0] = tensor_image[0][0].sub_(0.485).div_(0.229); //0通道数据变换
    tensor_image[0][1] = tensor_image[0][1].sub_(0.456).div_(0.224); //1通道数据变换
    tensor_image[0][2] = tensor_image[0][2].sub_(0.406).div_(0.225); //2通道数据变换

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor_image);
    auto output = m_model.forward(inputs).toTuple(); //张量数据处理

    torch::Tensor anomaly_map_T = output->elements()[0].toTensor(); //异物检测结果转换成张量
    int width = anomaly_map_T.sizes()[0];
    int height = anomaly_map_T.sizes()[1];
    anomaly_map_T = anomaly_map_T.mul(255).clamp(0, 255).to(torch::kU8);

    auto cuda_time = std::chrono::system_clock::now();
    anomaly_map_T = anomaly_map_T.to(torch::kCPU); //张量取回CPU

    auto post_infer_time = std::chrono::system_clock::now();
    std::cout << "post process cost time: " << std::chrono::duration_cast<std::chrono::milliseconds>(post_infer_time - cuda_time).count() << "ms" << std::endl;
    cv::Mat output_mat(height, width, CV_8UC1, anomaly_map_T.data_ptr()); //输出热力灰度图
    torch::jit::IValue pred_score = output->elements()[1];  //输出得分
    float pred_score_f = pred_score.toTensor().item<float>();

    auto end_infer_time = std::chrono::system_clock::now();
    std::cout << "only infer cost time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_infer_time - start_time).count() << "ms" << std::endl;
    m_score = pred_score_f;

    if (m_config_data.method_number == 4) //条包检测
    {
        if (m_score >= m_config_data.score_max_threshold)
        {
            cv::Mat ori_image = images[0];
            return tiaobao_post_process_Roi(output_mat, ori_image);
        }
    }
    else
    {
        if (m_score >= m_config_data.score_min_threshold)
        {
            cv::Mat ori_image = images[0];
            return  post_process_Roi(output_mat, ori_image);
        }
    }

    return roiInfo;
}

RoiInfo PatchCorePt::post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg)
{
    RoiInfo roiInfo;
    cv::Mat oriImgTmp = oriImg.clone();  //原始图副本
    cv::Mat heat_map_temp = heat_map.clone();  //热力图副本
    double Max_heat, Min_heat;
    cv::minMaxLoc(heat_map_temp, &Min_heat, &Max_heat); //获取热力最小值和热力最大值
    cv::Mat hist;  //输出灰度分布数组
    int histsize = Max_heat + 1;
    float ranges[] = {0, (float)Max_heat + 1};
    const float *histRanges = {ranges};
    cv::calcHist(&heat_map_temp, 1, 0, cv::Mat(), hist, 1, &histsize, &histRanges); //灰度分布直方图

    int max_step = 21;
    float num = 0;
    int value = 150;
    int value_thres = 0;
    for (int i = 0; i < max_step; i++)
    {
        num += hist.at<float>(Max_heat - i);
        if (num > 100)
        {
            value_thres = Max_heat - i;
            std::cout << "value_thres：" << value_thres << std::endl;
            if (value_thres > value)
            {
                value = value_thres;  //取出灰度大于150，且对应灰度的累积点数超过100的灰度
            }
            break;
        }
    }
    std::cout << "二值化阈值：" << value << std::endl;
    double minVal;
    double maxVal;
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(heat_map_temp, &minVal, &maxVal, &minLoc, &maxLoc);
    std::cout << "oriImg size: " << oriImg.size() << std::endl;
    cv::Mat heat_resize;
    cv::resize(heat_map_temp, heat_resize, cv::Size(oriImg.cols, oriImg.rows));  //热力图缩放，与原始图相同大小

    clock_t curr_time;
    curr_time = clock();
    // cv::imwrite("/home/HwHiAiUser/xiaoxiong/img/" + std::to_string(curr_time) + "heat.jpg", heat_resize);
    // cv::imwrite("/home/ai/Workspace/xiaoxiong/temporary_file/code/img/" + std::to_string(curr_time) + "heat.jpg", heat_resize);

    cv::Mat binary;
    cv::threshold(heat_resize, binary, value - 1, 255, cv::THRESH_BINARY); //二值化热力图

    cv::Mat diff;
    diff = cv::Mat(cv::Size(oriImg.cols, oriImg.rows), CV_8UC1, cv::Scalar(0));
    binary.copyTo(diff);
    std::cout << "binary size : " << binary.size() << std::endl;
    // 寻找疑似区域
    std::vector<std::vector<cv::Point>> maybe_contours;
    std::vector<cv::Rect> maybe_rois;
    std::vector<cv::Vec4i> hierachy;
    cv::findContours(diff, maybe_contours, hierachy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE, cv::Point(0, 0));  //从二值图像中提取轮廓
    std::cout << "疑似区域：" << maybe_contours.size() << std::endl;
    if (maybe_contours.size() == 0)
    {
        roiInfo.ltx = 0;
        roiInfo.lty = 0;
        roiInfo.rbx = 0;
        roiInfo.rby = 0;
    }
    else
    {
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
            for (int i = 0; i < maybe_rois.size(); i++)
            {
                if (maybe_rois[i].area() > max_area)
                {
                    max_area = maybe_rois[i].area();
                    n = i;
                }
            }
            std::cout << "最大区域值：" << max_area << std::endl;
            if (max_area >= 64)
            {
                roiInfo.ltx = std::max(maybe_rois[n].x, 0);
                roiInfo.lty = std::max(maybe_rois[n].y, 0);
                roiInfo.rbx = std::min(maybe_rois[n].x + maybe_rois[n].width, oriImg.cols);
                roiInfo.rby = std::min(maybe_rois[n].y + maybe_rois[n].height, oriImg.rows);
            }
            else
            {
                roiInfo.ltx = 0;
                roiInfo.lty = 0;
                roiInfo.rbx = 0;
                roiInfo.rby = 0;
            }
        }
        else
        {
            roiInfo.ltx = 0;
            roiInfo.lty = 0;
            roiInfo.rbx = 0;
            roiInfo.rby = 0;
        }
    }

    //阈值逻辑
    //&& (roiInfo.rbx - roiInfo.ltx) * (roiInfo.rby - roiInfo.lty) > 10000
    if (m_score >= m_config_data.score_max_threshold)
    {
        roiInfo.prob = -999;
    }
    else
    {
        if ((roiInfo.rbx - roiInfo.ltx) * (roiInfo.rby - roiInfo.lty) > 0 &&
            m_score >= m_config_data.score_min_threshold && m_score < m_config_data.score_max_threshold)
        {
            // std::cout
            if (value == 255 && (roiInfo.rbx - roiInfo.ltx) * (roiInfo.rby - roiInfo.lty) > 10000)
            {
                roiInfo.prob = -997;
                return roiInfo;
            }
            if ((roiInfo.rbx - roiInfo.ltx) * (roiInfo.rby - roiInfo.lty) > 3600)
            {
                //大区域缩减到30*30
                int width = roiInfo.rbx - roiInfo.ltx;
                int height = roiInfo.rby - roiInfo.lty;
                int center_point_x = roiInfo.ltx + width / 2;
                int center_point_y = roiInfo.lty + height / 2;

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

                roiInfo.ltx = std::max(center_point_x - width / 2, 0);
                roiInfo.lty = std::max(center_point_y - height / 2, 0);
                roiInfo.rbx = std::min(center_point_x + width / 2, oriImg.cols);
                roiInfo.rby = std::min(center_point_y + height / 2, oriImg.rows);
            }

            // ***模板匹配判断***
            int ex_dis = m_config_data.ex_pixel;

            cv::Rect rect(cv::Point(roiInfo.ltx, roiInfo.lty), cv::Point(roiInfo.rbx, roiInfo.rby));
            cv::Rect dst_rect(cv::Point(std::max(rect.x - ex_dis, 0),
                                        std::max(rect.y - ex_dis, 0)),
                              cv::Point(std::min(rect.br().x + ex_dis, oriImg.cols),
                                        std::min(rect.br().y + ex_dis, oriImg.rows)));
            cv::Mat src_gray;
            if (oriImg.channels() == 1)
            {
                src_gray = oriImgTmp;
            }
            else
            {
                cv::cvtColor(oriImgTmp, src_gray, cv::COLOR_BGR2GRAY);
            }
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            int index = 0;
            cv::Point locate;


            for (size_t j = 0; j < m_template_mat_vector.size(); ++j)
            {
                cv::Mat result;
                cv::Mat module_imgRoi = m_template_mat_vector[j](dst_rect);
                cv::cvtColor(module_imgRoi, module_imgRoi, cv::COLOR_BGR2GRAY);
                //归一化
                cv::matchTemplate(module_imgRoi, src_gray(rect), result, cv::TM_CCOEFF_NORMED);
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
                cv::Rect rect_module(maxLoc.x, maxLoc.y, rect.width, rect.height);
                cv::Mat diff_img, module_binary, ori_binary;
                ori_binary = m_template_mat_vector[index];
                cv::cvtColor(ori_binary, ori_binary, cv::COLOR_BGR2GRAY);
                cv::Mat ori_binary_rect = module_imgRoi(rect_module);
                cv::Mat src_gray_rect = src_gray(rect);

                cv::absdiff(ori_binary_rect, src_gray_rect, diff_img);
                cv::threshold(diff_img, module_binary, m_config_data.num_pixel - 1, 255, cv::THRESH_BINARY);
                cv::Scalar sum_value = cv::sum(module_binary);
                int mean_value = sum_value[0] / 255;
                if (mean_value > m_config_data.binary_threshold)
                {
                    roiInfo.prob = -998;
                }
                else
                {
                    roiInfo.prob = 1;
                    roiInfo.ltx = 0;
                    roiInfo.lty = 0;
                    roiInfo.rbx = 0;
                    roiInfo.rby = 0;
                    break;
                }
            }
        }
        else
        {
            roiInfo.prob = 1;
            roiInfo.ltx = 0;
            roiInfo.lty = 0;
            roiInfo.rbx = 0;
            roiInfo.rby = 0;
        }
    }
    return roiInfo;
}

RoiInfo PatchCorePt::tiaobao_post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg)
{
    std::cout<<"条包检测"<<std::endl;
    cv::Mat heat_map_gray = heat_map;
    cv::Mat oriImgTmp = oriImg.clone();
    RoiInfo roiInfo;
    cv::Mat heat_map_temp = heat_map_gray.clone();
    double Max_heat, Min_heat;
    cv::minMaxLoc(heat_map_temp, &Min_heat, &Max_heat);
    cv::Mat hist;
    int histsize = Max_heat + 1;
    float ranges[] = {0, (float)Max_heat + 1};
    const float *histRanges = {ranges};
    cv::calcHist(&heat_map_temp, 1, 0, cv::Mat(), hist, 1, &histsize, &histRanges);

    int max_step = 21;
    float num = 0;
    //热力图二值化阈值
    int value = m_config_data.heat_threshold;
    int value_thres = 0;
    for (int i = 0; i < max_step; i++)
    {
        num += hist.at<float>(Max_heat - i);
        if (num > 100)
        {
            value_thres = Max_heat - i;
            if (value_thres > value)
            {
                value = value_thres;
            }
            break;
        }
    }
    std::cout << "二值化阈值：" << value << std::endl;
    double minVal;
    double maxVal;
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(heat_map_temp, &minVal, &maxVal, &minLoc, &maxLoc);
    std::cout << "oriImg size: " << oriImg.size() << std::endl;
    cv::Mat heat_resize;
    cv::resize(heat_map_temp, heat_resize, cv::Size(oriImg.cols, oriImg.rows));
    cv::Mat binary;
    cv::threshold(heat_resize, binary, value - 1, 255, cv::THRESH_BINARY);

    cv::Mat diff;
    diff = cv::Mat(cv::Size(oriImg.cols, oriImg.rows), CV_8UC1, cv::Scalar(0));
    binary.copyTo(diff);
    std::cout << "binary size : " << binary.size() << std::endl;
    // 寻找疑似区域
    std::vector<std::vector<cv::Point>> maybe_contours;
    std::vector<cv::Rect> maybe_rois;
    std::vector<cv::Vec4i> hierachy;
    cv::findContours(diff, maybe_contours, hierachy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE, cv::Point(0, 0));
    std::cout << "疑似区域：" << maybe_contours.size() << std::endl;
    if (maybe_contours.size() == 0)
    {
        roiInfo.prob=0;
        roiInfo.ltx = 0;
        roiInfo.lty = 0;
        roiInfo.rbx = 0;
        roiInfo.rby = 0;
    }
    else
    {
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
            for (int i = 0; i < maybe_rois.size(); i++)
            {
                if (maybe_rois[i].area() > max_area)
                {
                    max_area = maybe_rois[i].area();
                    n = i;
                }
            }
            //最大区域
            if (max_area >= m_config_data.max_area)
            {
                std::cout << "最大区域值：" << max_area << std::endl;
                std::cout<<"预测得分："<<m_score<<std::endl;
                roiInfo.prob=m_score;
                roiInfo.ltx = std::max(maybe_rois[n].x, 0);
                roiInfo.lty = std::max(maybe_rois[n].y, 0);
                roiInfo.rbx = std::min(maybe_rois[n].x + maybe_rois[n].width, oriImg.cols);
                roiInfo.rby = std::min(maybe_rois[n].y + maybe_rois[n].height, oriImg.rows);
            }
            else
            {
                roiInfo.prob=0;
                roiInfo.ltx = 0;
                roiInfo.lty = 0;
                roiInfo.rbx = 0;
                roiInfo.rby = 0;
            }
        }
        else
        {
            roiInfo.prob=0;
            roiInfo.ltx = 0;
            roiInfo.lty = 0;
            roiInfo.rbx = 0;
            roiInfo.rby = 0;
        }
    }
    return roiInfo;
}
