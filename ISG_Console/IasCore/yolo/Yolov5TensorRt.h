#pragma once
#include "coreyolo.h"
#include "../tensorrt/TensorRt.h"

class Yolov5TensorRt : TensorRt
{
public:
    explicit Yolov5TensorRt(const std::string modelPath, const int batch_size = 1);  //构造函数
    ~Yolov5TensorRt();  //析构函数
    std::vector<std::vector<YoloResult>> infer(const std::vector<cv::Mat> &images);  //分类检测

private:
    int m_batch_size;  //batch推理size
    static constexpr int YOLO_IMAGE_WIDTH = 640;   //YOLO,图片缩放成640 * 640，进行目标检测
    static constexpr int YOLO_IMAGE_HEIGHT = 640;

public:
    float *m_cudaInputData;  //cuda检测输入缓冲区
    float *m_cudaOutputData;  //cuda检测输出缓冲区
};