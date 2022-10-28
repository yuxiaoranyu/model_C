#pragma once
#include <opencv2/opencv.hpp>
struct YoloResult
{
    int classId;  //类别Id
    float conf;   //置信度
    cv::Rect box; //区域
};
