#pragma once
#include "Yolov5TensorRt.h"
#include "OnnxYolo.h"

class Yolov5
{
public:
    Yolov5();  //构造函数
    ~Yolov5(); //析构函数

    bool init(const std::string &config_path, int gpu_type = 1);                                                        //初始化函数
    void cleanup();                                                                                                     //清理函数
    bool infer(const std::vector<cv::Mat> &images, std::vector<std::vector<YoloResult>> &yoloResult); //图片分类检测

private:
    Yolov5TensorRt *m_yolov5TensorRt; //模型管理对象
    OnnxYolo *m_yolov5Onnx;
};
