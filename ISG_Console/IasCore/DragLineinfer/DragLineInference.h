#pragma once

#include "../../IasBase/BaseAiInference.h"
#include "../../IasBase/BaseAIResult.h"
#include "../yolo/Yolov5.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonObject>

class DragLineInference : public BaseAiInference
{
public:
    DragLineInference(); //构造函数
    ~DragLineInference(); //析构函数

public:
    bool load(const std::string &param) override;  //加载
    bool update(const std::string &param) override; //更新
    std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) override; //推理
    bool remove() override;

private:
    struct YoloParam
    {
        YoloParam() { yolo_model = nullptr;}
        Yolov5 *yolo_model; //yolo检测，只进行目标检测，不进行目标分类
    };

private:
    YoloParam yoloparam; //yolo
    std::map<int, cv::Rect> detect_area;
    QJsonObject Camera_Params;
};

class DragLineInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new DragLineInference();
    }
};
