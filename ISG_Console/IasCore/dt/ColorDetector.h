#pragma once

#include "Detector.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class ColorDetector : public Detector
{
public:
    bool load(const std::string &param, const cv::Point2d &locate_point) override;
    BaseDTResult detect(const cv::Mat &frame, const cv::Point2d &locate_point) override;

private:
    //计算均值和标准偏差的差异，是否在设置范围内
    bool ContrastMeanStdDev(const cv::Mat &run_mean, const cv::Mat &this_mean,
                            const cv::Mat &run_stddev, const cv::Mat &this_stddev,
                            const cv::Mat &mean_range, const cv::Mat &stddev_range);
    std::string computeParam() override; //校正算法配置参数

private:
    double mean_gray, mean_b, mean_g, mean_r; //中心均值
    double stddev_gray, stddev_b, stddev_g, stddev_r; //中心标准偏差
    double mean_gray_range, mean_b_range, mean_g_range, mean_r_range; //均值范围
    double stddev_gray_range, stddev_b_range, stddev_g_range, stddev_r_range; //标准偏差范围
    bool on_gray; //是否将输入图片转成灰度图检测
};

class ColorDetectorFactory : public BaseDetectorFactory
{
public:
    BaseDetector *getDetector() override
    {
        return new ColorDetector();
    }
};
