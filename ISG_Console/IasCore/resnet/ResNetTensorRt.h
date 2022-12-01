#pragma once
#include "coreresnet.h"
#include "../tensorrt/TensorRt.h"

class ResNetTensorRt : TensorRt
{
private:
#define RESNET_IMAGE_WIDTH 224  //RESNET,图片缩放成224 * 224，进行分类检测
#define RESNET_IMAGE_HEIGHT 224
#define RESNET_OUTPUT_SIZE 1000 //RESNET，分配1000个空间，存放输出结果

public:
    explicit ResNetTensorRt(const std::string modelPath, const int batch_size);  //构造函数
    ~ResNetTensorRt(); //析构函数
    bool infer(const std::vector<cv::Mat> &images, std::vector<ResNetResult> &resnetResult); //分类检测
    bool getInit() {return m_bInitOk;}
private:
    void preprocessing(const cv::Mat &images, float *blob);  //分类检测预处理
    float m_cudaInputData[3 *RESNET_IMAGE_WIDTH * RESNET_IMAGE_HEIGHT]; //cuda检测输入缓冲区
    float m_cudaOutputData[RESNET_OUTPUT_SIZE];  //cuda检测输出缓冲区
};
