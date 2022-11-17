#pragma once
#include "corepatchcore.h"
#include <torch/torch.h>
#include "torch/script.h"

struct PatchCore_Forward_Result  // PatchCore推理结果数据结构
{
    float score;
    cv::Mat output_mat;
};

class PatchCorePt // PatchCorePytorch
{
public:
    explicit PatchCorePt(Config_Data &config, const int batch_size, const bool isGPU = true); //构造函数
    ~PatchCorePt();                                                                           //析构函数
    PatchCore_Result infer(const std::vector<cv::Mat> &images);                               //图片异常检测

private:
    PatchCore_Forward_Result forward(const std::vector<cv::Mat> &images);                                   // PatchCore推理
    bool findDefectArea(const cv::Mat &oriImg, const cv::Mat &heat_map, PatchCore_Result &patchCoreResult); //查找缺陷区域
    void post_process(const cv::Mat &oriImg, const cv::Mat &heat_map, PatchCore_Result &patchCoreResult);   //后处理
    bool matchTemplate(const cv::Mat &oriImg, PatchCore_Result &patchCoreResult);                           //模板匹配
    bool isInPolygon(const cv::Point &point, const std::vector<cv::Point> &points);                         //判别点是否在多边形区域内
    torch::jit::script::Module m_model;         //模型对象
    Config_Data m_config_data;                  //算法参数
    std::vector<cv::Mat> m_template_mat_vector; //模板图片vector
};
