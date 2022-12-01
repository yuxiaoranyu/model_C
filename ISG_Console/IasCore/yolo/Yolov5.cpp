#include "Yolov5.h"

Yolov5::Yolov5() : m_yolov5TensorRt(nullptr), m_yolov5Onnx(nullptr)
{
}

Yolov5::~Yolov5()
{
}

bool Yolov5::init(const std::string &config_path, int gpu_type)
{
    if (gpu_type == 0)
    {
        m_yolov5Onnx = new OnnxYolo(config_path, false);
    }
    else
    {
        m_yolov5TensorRt = new Yolov5TensorRt(config_path);

        if (!m_yolov5TensorRt->getInit())
        {
            return false;
        }
    }

    return true;
}

void Yolov5::cleanup()
{
    if (m_yolov5TensorRt != nullptr)
    {
        delete m_yolov5TensorRt;
        m_yolov5TensorRt = nullptr;
    }

    if (m_yolov5Onnx != nullptr)
    {
        delete m_yolov5Onnx;
        m_yolov5Onnx = nullptr;
    }
}

bool Yolov5::infer(const std::vector<cv::Mat> &images, std::vector<std::vector<YoloResult>> &yoloResult)
{
    if (m_yolov5Onnx != nullptr)
    {
        return m_yolov5Onnx->infer(images, yoloResult);
    }
    else
    {
        return m_yolov5TensorRt->infer(images, yoloResult);
    }
}
