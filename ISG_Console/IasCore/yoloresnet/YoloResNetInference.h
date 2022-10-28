#pragma once

#include "../../IasBase/BaseAiInference.h"
#include "../../IasBase/BaseAIResult.h"
#include "../yolo/Yolov5.h"
#include "../resnet/ResNet.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonArray>

class YoloResnetAiInference : public BaseAiInference
{
public:
    YoloResnetAiInference(); //构造函数
    ~YoloResnetAiInference(); //析构函数

public:
    bool load(const std::string &param) override;  //加载
    bool update(const std::string &param) override; //更新
    std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) override; //推理
    bool transformCoordinates(); //坐标转换矩阵构建
    bool remove() override;

    struct YoloParam
    {
        YoloParam() { yolo_model = nullptr; }

        Yolov5 *yolo_model;
        std::string model_path;
    };

    struct ResNetParam
    {
        ResNetParam() { resnet_model = nullptr; }
        ResNet *resnet_model;
        std::string model_path;
        QJsonArray label_array;
    };

private:
//    int m_gpu_type;  //GPU类型
//    bool m_run;      //是否启用推理
    YoloParam yoloparam;
    ResNetParam resnetparam;
    std::map<int, cv::Mat> m_transform_map;              //每个相机的转换矩阵
};

class YoloResnetAiInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new YoloResnetAiInference();
    }
};
