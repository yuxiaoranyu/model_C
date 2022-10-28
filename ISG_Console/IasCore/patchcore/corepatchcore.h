#pragma once
#include <opencv2/opencv.hpp>

struct Config_Data
{
    std::string model_path;                   //模型文件路径
    std::string template_path;                //模板图文件路径
    float confidence_threshold = 0.0;         //置信度阈值
    int ex_pixel;                             //模板匹配时，膨胀的像素点
    int min_area;                             //最小区域面积
    int max_area = 100;                       //最大区域面积
    int num_pixel;                           // 点数目,作为灰度图阈值，对灰度图像进行阈值操作得到二值图像

    int binary_threshold;                    //二进制阈值
    float score_min_threshold = 0.0;         //得分最小阈值
    float score_max_threshold = 0.0;        //得分最大阈值
    int heat_threshold = 150;               //热力度阈值
    int method_number = 0;                   //检测类别
    std::vector<cv::Point> effective_area;  //有效区域
};

struct RoiInfo
{
    RoiInfo() : prob(0), ltx(0), lty(0), rbx(0), rby(0) {}

    double prob; //预测得分
    int ltx; //left_top_x  左上角
    int lty;
    int rbx; //right_bottom_x 右上角
    int rby;
};
