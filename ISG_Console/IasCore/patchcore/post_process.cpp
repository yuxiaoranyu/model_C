#include "post_process.h"
#include <time.h>
#include <unistd.h>

bool isInPolygon(const cv::Point &point, const std::vector<cv::Point> &points)
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

RoiInfo post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info)
{
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

    clock_t curr_time;
    curr_time = clock();
    // cv::imwrite("/home/HwHiAiUser/xiaoxiong/img/" + std::to_string(curr_time) + "heat.jpg", heat_resize);
    // cv::imwrite("/home/ai/Workspace/xiaoxiong/temporary_file/code/img/" + std::to_string(curr_time) + "heat.jpg", heat_resize);

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
    if (Input_info.score >= Input_info.score_max_threshold)
    {
        roiInfo.prob = -999;
    }
    else
    {
        if ((roiInfo.rbx - roiInfo.ltx) * (roiInfo.rby - roiInfo.lty) > 0 &&
            Input_info.score >= Input_info.score_min_threshold && Input_info.score < Input_info.score_max_threshold)
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
            int ex_dis = Input_info.ex_pixel;

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


            for (size_t j = 0; j < Input_info.module_img.size(); ++j)
            {
                cv::Mat result;
                cv::Mat module_imgRoi = Input_info.module_img[j](dst_rect);
                cv::cvtColor(module_imgRoi, module_imgRoi, cv::COLOR_BGR2GRAY);
                //归一化
                cv::matchTemplate(module_imgRoi, src_gray(rect), result, cv::TM_CCOEFF_NORMED);
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
                cv::Rect rect_module(maxLoc.x, maxLoc.y, rect.width, rect.height);
                cv::Mat diff_img, module_binary, ori_binary;
                ori_binary = Input_info.module_img[index];
                cv::cvtColor(ori_binary, ori_binary, cv::COLOR_BGR2GRAY);
                cv::Mat ori_binary_rect = module_imgRoi(rect_module);
                cv::Mat src_gray_rect = src_gray(rect);

                cv::absdiff(ori_binary_rect, src_gray_rect, diff_img);
                cv::threshold(diff_img, module_binary, Input_info.num_pixel - 1, 255, cv::THRESH_BINARY);
                cv::Scalar sum_value = cv::sum(module_binary);
                int mean_value = sum_value[0] / 255;
                if (mean_value > Input_info.binary_threshold)
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

RoiInfo tiaobao_post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg, InputInfo &Input_info)
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
    int value = Input_info.heat_threshold;
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
            if (max_area >= Input_info.max_area)
            {
                std::cout << "最大区域值：" << max_area << std::endl;
                std::cout<<"预测得分："<<Input_info.score<<std::endl;
                roiInfo.prob=Input_info.score;
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
