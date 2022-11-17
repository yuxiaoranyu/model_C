#pragma once
#include <opencv2/opencv.hpp>

struct Config_Data
{
    Config_Data() : score_min_threshold(0),
                    score_max_threshold(0),
                    heat_threshold(150),
                    heat_binary_min_area(100),
                    defective_min_area(64),
                    btemplate_match(false),
                    expand_pixel(3),
                    diff_threshold(50),
                    dismatched_point_threshold(50) {}

    std::string model_path;                //模型文件路径
    std::string template_path;             //模板图文件路径
    float score_min_threshold;             //得分最小阈值
    float score_max_threshold;             //得分最大阈值
    int heat_threshold;                    //热力度阈值
    int heat_binary_min_area;              //热力图中灰度大于heat_threshold的最少累积点数
    int defective_min_area;                //最小允许的缺陷框大小
    std::vector<cv::Point> effective_area; //有效区域
    bool btemplate_match;                  //是否开展模板匹配
    int expand_pixel;                      //模板匹配时，膨胀的像素点
    int diff_threshold;                    //原图和模板图做差后的颜色二值化阈值 (0~255)
    int dismatched_point_threshold;        //原图和模板图做差二值化后的为1的点数阈值
};

struct PatchCore_Result
{
    PatchCore_Result() : result(true), patchcore_score(0), ltx(0), lty(0), rbx(0), rby(0) {}

    bool result;
    float patchcore_score;
    int ltx;                   // left_top_x  左上角
    int lty;
    int rbx; // right_bottom_x 右上角
    int rby;
};
