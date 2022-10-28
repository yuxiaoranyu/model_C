#pragma once
#include "corepatchcore.h"
#include <torch/torch.h>
#include "torch/script.h"

class PatchCorePt
{
public:

    explicit PatchCorePt(Config_Data &config, const int batch_size, const bool isGPU = true); //构造函数
    ~PatchCorePt(); //析构函数
    RoiInfo infer(const std::vector<cv::Mat> &images); //图片异常检测

private:
    Config_Data m_config_data; //算法参数
    torch::jit::script::Module m_model; //模型对象
    std::vector<cv::Mat> m_template_mat_vector; //模板图片vector
    float m_score;

    RoiInfo tiaobao_post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg); //后处理
    RoiInfo post_process_Roi(cv::Mat &heat_map, cv::Mat &oriImg); //后处理
};
