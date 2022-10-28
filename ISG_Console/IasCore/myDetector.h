#ifndef MYDETECTOR_H
#define MYDETECTOR_H

#include "IasBase/BaseDetector.h"
#include "IasBase/BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class MyDetector : public BaseDetector
{
public:
    MyDetector() {};
    ~MyDetector() {};

public:
    bool load(const std::string &param, const cv::Point2d& locate_point) override
    {
        return true;
    }
    bool remove() override { return true; }
    bool update(const std::string &param) override { return true; }
    bool reset(std::string &result) override { return true; }
    bool exec(std::string &result) override { return true; }
    int getLocateIdX() override { return 1; }
    int getLocateIdY() override { return 1; }
    void setLocateRoot(const cv::Point2d& locate_point) override {}
    BaseDTResult detect(const cv::Mat& frame, const cv::Point2d& locate_point) override
    {
        BaseDTResult aa;

        aa.setId(getId());
        aa.setResult(true);
        return aa;
    }
};

class MyDetectorFactory : public BaseDetectorFactory
{
public:
    BaseDetector *getDetector() override
    {
        return new MyDetector();
    }
};
#endif // MYDETECTOR_H
