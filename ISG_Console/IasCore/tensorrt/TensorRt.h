#pragma once
#include <opencv2/opencv.hpp>
#include "NvInfer.h"
#include <cuda_runtime_api.h>

class TensorRt
{
public:
    explicit TensorRt(const std::string modelPath, const size_t inSize, const size_t outSize);  //构造函数
    ~TensorRt(); //析构函数
    void doInference(const int batchSize, float *input_data, const size_t input_size, float *output_data, const size_t output_size);

public:
    bool m_bInitOk; //初始化成功标记

private:
    nvinfer1::IRuntime *m_runtime; //将 CudaEngine 的序列化缓存文件反序列化回来
    nvinfer1::ICudaEngine *m_engine; //推理引擎
    nvinfer1::IExecutionContext *m_context;  //推理引擎运行上下文
    cudaStream_t m_stream; //CUDA流(表示一个GPU操作队列，该队列中的操作将以添加到流中的先后顺序而依次执行)
    void *m_buffers[2]; //GPU内存分配
};
