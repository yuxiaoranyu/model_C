#pragma once

#include "IasBase/BaseDetector.h"
#include "IasBase/BaseDTResult.h"
#include <QJsonObject>
#include <string>
#include <opencv2/opencv.hpp>

class Detector : public BaseDetector
{
public:
    Detector(); //构造函数
    ~Detector(); //构造函数
    bool update(const std::string &param) override;  //参数更新
    bool remove() override;
    bool resetParam(std::string &result) override; //将算法的配置恢复到算法推荐值
    bool exec(std::string &result) override;
    int getLocateIdX() override; //获取X定位器的ID
    int getLocateIdY() override;  //获取Y定位器的ID
    virtual std::string computeParam() = 0;  //校正算法配置参数
    void setLocateRoot(const cv::Point2d &locate_point) override; //设置静态定位点
    cv::Rect2d getModifiedRect(const cv::Point2d &locate_point);

protected:
    bool m_run;                 //是否使能捡测
    bool m_InitOK;              //状态
    int m_locate_x_id;          //x方向定位框id
    int m_locate_y_id;          //y方向定位框id
    cv::Rect2d m_rect_roi;      //检测区域，来自json配置
    cv::Point2d m_locate_root;  //静态定位点
    std::string m_config;       //配置信息
    QJsonObject m_resultJson;   //检测结果json
};


