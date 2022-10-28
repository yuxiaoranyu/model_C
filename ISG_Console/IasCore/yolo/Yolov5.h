#pragma once
#include "Yolov5TensorRt.h"

class Yolov5
{
public:
    Yolov5();  //构造函数
    ~Yolov5();  //析构函数

    bool init(const std::string &config_path);  //初始化函数
    void cleanup(); //清理函数
    std::vector<std::vector<YoloResult>> infer(const std::vector<cv::Mat> &images); //图片分类检测

private:
    Yolov5TensorRt *m_yolov5TensorRt;  //模型管理对象
};

