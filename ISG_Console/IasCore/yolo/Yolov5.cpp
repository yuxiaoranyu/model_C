#include "Yolov5.h"

Yolov5::Yolov5() :
    m_yolov5TensorRt(nullptr)
{
}

Yolov5::~Yolov5()
{
}

bool Yolov5::init(const std::string &config_path)
{
   m_yolov5TensorRt = new Yolov5TensorRt(config_path);

   return true;
}

void Yolov5::cleanup()
{
    if (m_yolov5TensorRt != nullptr) {
        delete m_yolov5TensorRt;
        m_yolov5TensorRt = nullptr;
    }
}

std::vector<std::vector<YoloResult>> Yolov5::infer(const std::vector<cv::Mat> &images)
{
   return m_yolov5TensorRt->infer(images);
}
