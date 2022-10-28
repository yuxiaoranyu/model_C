#ifndef BASEDTRESULT_H
#define BASEDTRESULT_H

#include <string>
#include <opencv2/opencv.hpp>

class BaseDTResult
{
public:
    BaseDTResult() : m_id(-1), m_result(true) {};
    virtual ~BaseDTResult() {};

public:
    int getId() const { return m_id; }
    void setId(const int id) { m_id = id; }

    bool getResult() const { return m_result; }
    void setResult(const bool result) { m_result = result; }

    cv::Point2d getLocatePoint() const { return m_locate_point; }
    void setLocatePoint(const cv::Point2d& locate_point) { m_locate_point = locate_point; }

    std::string getPositionInfo() const { return m_position_info; }
    void setPositionInfo(const std::string position_info) { m_position_info = position_info; }

private:
    int m_id; //IOP界面呈现的算法ID
    bool m_result;  //算法执行结果
    cv::Point2d m_locate_point;  //定位器的定位点
    std::string m_position_info; //检测结果字符信息
};

#endif // BASEDTRESULT_H
