#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <utility>
#include "onnx_utils.h"
#include "coreyolo.h"

class OnnxYolo
{
public:
    OnnxYolo(const std::string &model_path, const bool use_gpu = true);
    bool infer(const std::vector<cv::Mat> &images, std::vector<std::vector<YoloResult>> &yoloResult);

private:
    void preprocessing(const std::vector<cv::Mat> &images, float *blob, std::vector<cv::Size> &original_size);
    std::vector<std::vector<YoloResult>> postprocessing(const std::vector<cv::Size> &original_size, std::vector<Ort::Value> &output_tensors);
    static void getBestClassInfo(std::vector<float>::iterator it, const int numClasses, float &bestConf, int &bestClassId);
    void getBoxesAndConfs(const std::vector<float> &dst, const std::vector<cv::Size> &original_size, std::vector<std::vector<YoloResult>> &detections);

    // ort
    Ort::Session m_ort_session{nullptr};

    // model attribute
    std::vector<const char *> m_ort_input_names;
    std::vector<const char *> m_ort_output_names;
    std::vector<int64_t> m_ort_input_tensor_shape;
    std::vector<int64_t> m_ort_output_tensor_shape;

    size_t m_input_tensor_size;

    cv::Size2f m_resized_image_size;

    float _conf_thres = 0.75;
    float _iou_thres = 0.45;
};
