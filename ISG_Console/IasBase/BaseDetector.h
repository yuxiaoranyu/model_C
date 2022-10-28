#ifndef BASEDETECTOR_H
#define BASEDETECTOR_H

#include "BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class BaseDetector
{
public:
    BaseDetector() : m_id(-1) {};
    virtual ~BaseDetector() {};

public:
    virtual bool load(const std::string &param, const cv::Point2d& locate_point) = 0; //设置加载参数
    virtual bool update(const std::string &param) = 0;  //参数更新
    virtual void setLocateRoot(const cv::Point2d& locate_point) = 0;  //设置检测器使用的定位点
    virtual BaseDTResult detect(const cv::Mat& frame, const cv::Point2d& locate_point) = 0;  //检测
    virtual bool exec(std::string &result) = 0; //使用关联Locator配置的定位点，对静态图执行一次检测
    virtual bool reset(std::string &result) = 0;  //将一个检测算法的配置恢复到算法推荐值，并返回
    virtual bool remove() = 0; //对象删除前的资源回收
    virtual int getLocateIdX() = 0; //获取X方向锚定的Locator ID
    virtual int getLocateIdY() = 0; //获取Y方向锚定的Locator ID

    int getId() const { return m_id; }
    void setId(const int id) { m_id = id; }

    std::string getPath() const { return m_path; }
    void setPath(const std::string path) { m_path = path; }

private:
    int m_id;  //IOP界面呈现的算法ID
    std::string m_path; //算法配置文件全路径
    std::string m_brandPath; //牌号路径
};

class BaseDetectorFactory
{
public:
    BaseDetectorFactory() {};
    virtual ~BaseDetectorFactory() {};

public:
    virtual BaseDetector *getDetector() = 0;
};

void RegisterBaseDetectorFactory(int method, BaseDetectorFactory *detectorFactory);
BaseDetectorFactory *FindBaseDetectorFactory(int method);

#endif // BASEDETECTOR_H
