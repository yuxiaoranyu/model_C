#pragma once

#include "Detector.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class LineDetector : public Detector
{
public:
    bool load(const std::string &param, const cv::Point2d &locate_point) override;
    BaseDTResult detect(const cv::Mat &frame, const cv::Point2d &locate_point) override;

private:
    bool getLine(const cv::Mat &frame, double result_data[7]);  //线查找
    std::string computeParam() override; //校正算法配置参数

private:
    double line_length_range;  //以检测框中两个定位点长度乘以配置值作为线查找的最短长度要求，长度阈值百分比，范围为0~1；
    double binary_threshold;  //边缘检测阈值，范围为0~255；
    double line_score_threshold;  //线查找的得分阈值得分，只有高于设置阈值才能算OK
    double line_length;  //检测框中两个定位点长度，由两个定位点位置，动态计算获取
    double line_angle_range[2]; //烟体边缘角度范围起始角度，由两个定位点的位置，动态计算获取；
                                //烟体边缘角度范围结束角度，界面配置；

    std::vector<cv::Point2d> line_point = {cv::Point2d(0, 0), cv::Point2d(0, 0)};  //检测框中两个定位点
};

class LineDetectorFactory : public BaseDetectorFactory
{
public:
    BaseDetector *getDetector() override
    {
        return new LineDetector();
    }
};
