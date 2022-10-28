#ifndef BASEAIINFERENCE_H
#define BASEAIINFERENCE_H

#include "BaseAIResult.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

//ISG与IAS接口定义
class BaseAiInference
{
public:
    BaseAiInference() : m_id(-1) {};
    virtual ~BaseAiInference() {};

public:
//    virtual void addCameraId(int cameraId) = 0;  //添加AI管理的相机
    virtual bool load(const std::string &param) = 0;  //设置加载参数
    virtual bool update(const std::string &param) = 0;  //参数更新
    virtual std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) = 0;  //推理
    virtual bool remove() = 0;  //对象删除前的资源回收

    int getId() const { return m_id; }
    void setId(const int id) { m_id = id; }

    std::string getPath() const { return m_path; }
    void setPath(const std::string path) { m_path = path; }

private:
    int m_id;  //IOP界面呈现的算法ID
    std::string m_path; //算法配置文件全路径
};

class BaseAiInferenceFactory
{
public:
    BaseAiInferenceFactory() {};
    virtual ~BaseAiInferenceFactory() {};

public:
    virtual BaseAiInference *getAiInference() = 0;
};

void RegisterBaseAiInferenceFactory(int componentType, BaseAiInferenceFactory *aiInferenceFactory); //算法注册接口
BaseAiInferenceFactory *FindBaseAiInferenceFactory(int componentType);

#endif // BASEAIINFERENCE_H
