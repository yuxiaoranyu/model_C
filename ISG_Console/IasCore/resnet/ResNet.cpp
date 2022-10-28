#include "ResNet.h"

ResNet::ResNet() :
    m_resnetTensorRt(nullptr)
{
}

ResNet::~ResNet()
{
}

bool ResNet::init(const std::string &config_path)
{
    m_resnetTensorRt = new ResNetTensorRt(config_path, 1);

    return true;
}

void ResNet::cleanup()
{
    if (m_resnetTensorRt != nullptr) {
        delete m_resnetTensorRt;
        m_resnetTensorRt = nullptr;
    }
}

std::vector<ResNetResult> ResNet::infer(const std::vector<cv::Mat> &images)
{
    std::vector<ResNetResult> results = m_resnetTensorRt->infer(images);

    return results;
}
