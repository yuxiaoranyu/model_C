#ifndef BASEAIRESULT_H
#define BASEAIRESULT_H

#include <string>
#include <opencv2/opencv.hpp>

class BaseAIResult
{
public:
    BaseAIResult() : m_result(true) {};
    virtual ~BaseAIResult() {};

public:
    bool getResult() const { return m_result; }
    void setResult(const bool result) { m_result = result; }

    std::string getPositionInfo() const { return m_position_info; }
    void setPositionInfo(const std::string position_info) { m_position_info = position_info; }

    std::string getIsgInfo() const { return m_isg_info; }
    void setIsgInfo(const std::string isg_info) { m_isg_info = isg_info; }

private:
    bool m_result;  //算法执行结果
    std::string m_position_info; //检测结果iop字符信息
    std::string m_isg_info; //检测结果isg字符信息
};

#endif // BASEAIRESULT_H
