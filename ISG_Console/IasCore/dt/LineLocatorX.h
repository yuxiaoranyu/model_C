#pragma once

#include "IasBase/BaseLocator.h"
#include "Locator.h"
#include "IasBase/BaseDTResult.h"
#include <QJsonObject>
#include <string>
#include <opencv2/opencv.hpp>

class LineLocatorX : public Locator
{
public:
    bool load(const std::string &param) override; //加载
    BaseDTResult locate(const cv::Mat &mat) override; //定位

private:
    bool getLocateLinePoint(const cv::Mat &frame, double &score, cv::Point &locatePoint);  //用Hough查找定位线，取中点做定位点
    std::string computeParam() override; //校正算法配置参数

private:
    double locate_direction;    //在定位烟体左边缘或右边缘时,可能出现多条线段，选择最左侧的线作为定位线还是最右侧的线作为定位线；范围[0、1]；
    double line_length_range;   //以定位框ROI高度乘以配置值作为线查找的最短长度要求，长度阈值百分比，范围为0~1；
    double canny_threshold;    //边缘检测阈值，范围为0~255；
    double line_angle_begin;    //烟体边缘角度范围起始角度，范围为0~180；
    double line_angle_end;      //烟体边缘角度范围结束角度，范围为0~180；
    double line_score_threshold;//线查找的得分阈值得分，只有高于设置阈值才能算OK
};

class LineLocatorXFactory : public BaseLocatorFactory
{
public:
    LineLocatorX *getLocator()
    {
        return new LineLocatorX();
    }
};

