#pragma once

#include "Detector.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class TemplateDetector : public Detector
{
public:
    bool load(const std::string &param, const cv::Point2d &locate_point) override;
    BaseDTResult detect(const cv::Mat &frame, const cv::Point2d &locate_point) override;

private:
    bool getMatchTemplate(const cv::Mat &frame, double result_data[9]); //模板匹配
    std::string computeParam() override; //校正算法配置参数

private:
    cv::Mat template_img;
    double matching_score_threshold;
};

class TemplateDetectorFactory : public BaseDetectorFactory
{
public:
    BaseDetector *getDetector() override
    {
        return new TemplateDetector();
    }
};
