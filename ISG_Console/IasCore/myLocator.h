#ifndef MYLOCATOR_H
#define MYLOCATOR_H

#include "IasBase/BaseLocator.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class MyLocator : public BaseLocator
{
public:
    MyLocator() {}
    ~MyLocator() {}

public:
    bool load(const std::string &param) override
    {
        return true;
    }
    bool remove() override { return true; }
    bool update(const std::string &param) override{ return true; }
    BaseDTResult locate(const cv::Mat &mat) override
    {
        BaseDTResult aa;

        aa.setId(getId());
        aa.setResult(true);
        return aa;
    }
    bool reset(std::string &result) override { return true; }
    bool exec(std::string &result) override { return true; }
    cv::Point2d getLocateRoot() override { cv::Point2d p(0, 0); return p;}
};

class MyLocatorFactory : public BaseLocatorFactory
{
public:
    BaseLocator *getLocator()
    {
        return new MyLocator();
    }
};

#endif // MYLOCATOR_H
