#pragma once

#include "../../IasBase/BaseAiInference.h"
#include "../../IasBase/BaseAIResult.h"
#include "../yolo/Yolov5.h"
#include "../resnet/ResNet.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonArray>

class MixBrandInference : public BaseAiInference
{
public:
    MixBrandInference(); //构造函数
    ~MixBrandInference(); //析构函数

public:
    bool load(const std::string &param) override;  //加载
    bool update(const std::string &param) override; //更新
    std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) override; //推理
    bool remove() override;

private:
    struct YoloParam
    {
        YoloParam() { yolo_model = nullptr; }

        std::string model_path;  //模型文件全路径
        Yolov5 *yolo_model; //yolo检测，只进行目标检测，不进行目标分类
    };

    struct ResNetParam
    {
        ResNetParam() { resnet_model = nullptr; }

        std::string model_path; //模型文件全路径
        QJsonArray label_array; //resnet配置的分类ID及参数
        ResNet *resnet_model; //resnet检测
    };

private:
    YoloParam yoloparam; //yolo
    ResNetParam resnetparam;
};

class MixBrandInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new MixBrandInference();
    }
};
