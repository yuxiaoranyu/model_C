#pragma once
#include "PatchCorePt.h"
#include "corepatchcore.h"

class PatchCore
{
public:
    PatchCore();  //构造函数
    ~PatchCore(); //析构函数

    bool init(Config_Data &config);                             //初始化函数
    void cleanup();                                             //清理函数
    PatchCore_Result infer(const std::vector<cv::Mat> &images); //图片异常检测

private:
    PatchCorePt *m_patchcore_pt; // PatchCore模型管理对象
};
