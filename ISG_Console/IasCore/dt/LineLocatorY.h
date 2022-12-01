#pragma once

#include "Locator.h"
#include "IasBase/BaseDTResult.h"
#include <QJsonObject>
#include <string>
#include <opencv2/opencv.hpp>

class LineLocatorY : public Locator
{
public:
    bool load(const std::string &param) override; //加载
    BaseDTResult locate(const cv::Mat &mat) override; //定位

private:
    bool getLocateLinePoint(const cv::Mat &frame, double &score, cv::Point &locatePoint); //用Hough查找定位线，线取中点做定位点
    std::string computeParam() override; //校正算法配置参数

private:
    double locate_direction;    //在定位烟体上下边缘时,可能出现多条线段，选择最上侧的线作为定位线还是最下侧的线作为定位线；范围[0、1]；
    double line_length_range;   //以检测框中两个定位点长度乘以配置值作为线查找的最短长度要求，长度阈值百分比，范围为0~1；
    double binary_threshold;    //边缘检测阈值，范围为0~255；
    double line_length;         //检测框中两个定位点长度，由两个定位点位置，动态计算获取
    double line_angle_range[2]; //烟体边缘角度范围起始角度，由两个定位点的位置，动态计算获取；
                                //烟体边缘角度范围结束角度，界面配置；
    double line_score_threshold;//线查找的得分阈值得分，只有高于设置阈值才能算OK
    //double line_score_method;   //线计分方法 1.不区分线条背景 2.对暗色背景明亮线更敏感  3.对明亮背景暗色线更敏感  未落地

    std::vector<cv::Point2d> line_point = {cv::Point2d(0, 0), cv::Point2d(0, 0)};  //检测框中两个定位点
};

class LineLocatorYFactory : public BaseLocatorFactory
{
public:
    LineLocatorY *getLocator()
    {
        return new LineLocatorY();
    }
};

