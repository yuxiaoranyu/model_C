#pragma once

#include "Locator.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>
#include <QJsonObject>

class DefaultLocator : public Locator
{
public:
    bool load(const std::string &param) override; //加载
    BaseDTResult locate(const cv::Mat &mat) override; //定位

private:
    std::string computeParam() override; //校正算法配置参数
};

class DefaultLocatorFactory : public BaseLocatorFactory
{
public:
    BaseLocator *getLocator()
    {
        return new DefaultLocator();
    }
};
