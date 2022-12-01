#pragma once
#include "ResNetTensorRt.h"

class ResNet
{
public:
    ResNet();  //构造函数
    ~ResNet();  //析构函数
    bool init(const std::string &config_path);  //初始化函数
    void cleanup(); //清理函数
    bool infer(const std::vector<cv::Mat> &images, std::vector<ResNetResult> &resnetResult); //推理

private:
    ResNetTensorRt *m_resnetTensorRt; //ResNet模型管理对象
};

