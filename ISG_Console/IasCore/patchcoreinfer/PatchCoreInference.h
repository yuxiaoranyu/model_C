#ifndef PATCHCOREINFERENCE_H
#define PATCHCOREINFERENCE_H

#endif // PATCHCOREINFERENCE_H
#pragma once

#include "../../IasBase/BaseAiInference.h"
#include "../../IasBase/BaseAIResult.h"
#include "../patchcore/PatchCore.h"
#include "../patchcore/corepatchcore.h"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonArray>

class PatchCoreInference : public BaseAiInference
{
public:
    PatchCoreInference();  //构造函数
    ~PatchCoreInference(); //析构函数

public:
    bool load(const std::string &param) override;                                                              //加载
    bool update(const std::string &param) override;                                                            //更新
    std::vector<BaseAIResult> infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images) override; //推理
    bool remove() override;

private:
    std::map<int, PatchCore *> m_camera_map;
};

class PatchCoreInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new PatchCoreInference();
    }
};
