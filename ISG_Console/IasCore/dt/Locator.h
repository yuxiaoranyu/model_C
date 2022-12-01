#pragma once

#include "IasBase/BaseLocator.h"
#include "IasBase/BaseDTResult.h"
#include <QJsonObject>
#include <string>
#include <opencv2/opencv.hpp>

class Locator : public BaseLocator
{
public:
    Locator(); //构造函数
    ~Locator(); //构造函数
    bool update(const std::string &param) override;  //参数更新
    bool resetParam(std::string &result) override; //将算法的配置恢复到算法推荐值
    bool exec(std::string &result) override;  //执行一个定位算法的处理，返回静态图检测结果
    bool remove() override;  //对象删除前的资源回收
    virtual std::string computeParam() = 0;  //校正算法配置参数
    cv::Point2d getLocateRoot() override;  //静态配置的定位点
    void setLocateRoot(const cv::Point2d &locate_root);  //设置静态定位点

protected:
    bool m_run;                 //是否使能捡测
    bool m_InitOK;              //状态

    cv::Rect2d m_rect_roi;      //检测区域，来自json配置
    std::string m_config;       //配置信息
    QJsonObject m_resultJson;   //检测结果json

private:
    cv::Point2d m_locate_root;   //静态定位点
};


