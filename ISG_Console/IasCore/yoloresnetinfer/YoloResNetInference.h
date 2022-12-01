#pragma once

#include "../../IasBase/BaseAiInference.h"
#include "../../IasBase/BaseAIResult.h"
#include "../yolo/Yolov5.h"
#include "../resnet/ResNet.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonArray>

class YoloResnetInference : public BaseAiInference
{
public:
    YoloResnetInference(); //构造函数
    ~YoloResnetInference(); //析构函数

public:
    bool load(const std::string &param) override;  //加载
    bool update(const std::string &param) override; //更新
    std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) override; //推理
    bool remove() override;

private:
    bool transformCoordinates(); //坐标转换矩阵构建
    struct YoloParam
    {
        YoloParam() { yolo_model = nullptr; }
        Yolov5 *yolo_model; //yolo检测，只进行目标检测，不进行目标分类
    };

    struct ResNetParam
    {
        ResNetParam() { resnet_model = nullptr; }
        QJsonArray label_array; //resnet配置的分类ID及参数
        ResNet *resnet_model; //resnet检测
    };

private:
//    int m_gpu_type;  //GPU类型
//    bool m_run;      //是否启用推理
    YoloParam yoloparam; //yolo
    ResNetParam resnetparam;
    std::map<int, cv::Mat> m_transform_map;    //每个相机的转换矩阵
};

class YoloResnetInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new YoloResnetInference();
    }
};
