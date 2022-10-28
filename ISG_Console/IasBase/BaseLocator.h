#ifndef BASELOCATOR_H
#define BASELOCATOR_H

#include "BaseDTResult.h"
#include <string>
#include <opencv2/opencv.hpp>

class BaseLocator
{
public:
    BaseLocator() : m_id(-1) {};
    virtual ~BaseLocator() {};

public:
    virtual bool load(const std::string &param) = 0;  //设置加载参数
    virtual bool update(const std::string &param) = 0;  //参数更新
    virtual BaseDTResult locate(const cv::Mat &mat) = 0;  //定位
    virtual bool exec(std::string &result) = 0; //执行一个定位算法的处理，返回静态图检测结果
    virtual bool reset(std::string &result) = 0;  //将一个定位算法的配置恢复到算法推荐值，并返回
    virtual bool remove() = 0; //对象删除前的资源回收

    virtual cv::Point2d getLocateRoot() = 0;  //静态配置的定位点

    int getId() const { return m_id; }
    void setId(const int id) { m_id = id; }

    std::string getPath() const { return m_path; }
    void setPath(const std::string path) { m_path = path; }

private:
    int m_id;  //IOP界面呈现的算法ID
    std::string m_path; //算法配置文件全路径
};

class BaseLocatorFactory
{
public:
    BaseLocatorFactory() {};
    virtual ~BaseLocatorFactory() {};

public:
    virtual BaseLocator *getLocator() = 0;
};

void RegisterBaseLocatorFactory(int method, BaseLocatorFactory *locatorFactory);
BaseLocatorFactory *FindBaseLocatorFactory(int method);

#endif // BASELOCATOR_H
