#include "TensorRt.h"
#include "logging.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace nvinfer1;

TensorRt::TensorRt(const string modelPath, const size_t inSize, const size_t outSize) :
    m_bInitOk(false),
    m_runtime(nullptr),
    m_engine(nullptr),
    m_context(nullptr)
{
    static Logger gLogger;

    std::cout<<"tensorRt model path "<<modelPath<<std::endl;
    std::ifstream file(modelPath, std::ios::binary);
    if (file.good())
    {
        file.seekg(0, file.end);
        size_t size = file.tellg();

        char *streambuf = new char[size];

        file.seekg(0, file.beg);
        file.read(streambuf, size);
        file.close();

        m_runtime = createInferRuntime(gLogger);
        if (m_runtime == nullptr)
        {
            return;
        }

        m_engine = m_runtime->deserializeCudaEngine(streambuf, size, nullptr);
        if (m_engine == nullptr)
        {
            return;
        }

        m_context = m_engine->createExecutionContext();
        if (m_context == nullptr)
        {
            return;
        }

        delete[] streambuf;

        if (cudaSuccess != cudaMalloc(&m_buffers[0], inSize ))
        {
            return;
        }
        if (cudaSuccess != cudaMalloc(&m_buffers[1], outSize ))
        {
            return;
        }
        if (cudaSuccess != cudaStreamCreate(&m_stream))
        {
            return;
        }

        m_bInitOk = true;
        std::cout<<"tensorRt init ok"<<std::endl;
    }
    else
    {
        std::cerr << "tensorRt init failed! " << std::endl;
    }
}

TensorRt::~TensorRt()
{
    if (m_bInitOk)
    {
        cudaStreamDestroy(m_stream);
        cudaFree(m_buffers[0]);
        cudaFree(m_buffers[1]);
        m_context->destroy();
        m_engine->destroy();
        m_runtime->destroy();
    }
}

//CUDA调用
void TensorRt::doInference(const int batchSize, float *input_data, const size_t input_size, float *output_data, const size_t output_size)
{
    //CUDA流表示一个GPU操作队列，该队列中的操作将以添加到流中的先后顺序而依次执行

    //内存空间复制（待推理图像数据复制到CUDA内存空间m_buffers[0]），不确保cudaMemcpyAsync函数返回时已经启动了复制动作
    if (cudaSuccess != cudaMemcpyAsync(m_buffers[0], input_data, input_size, cudaMemcpyHostToDevice, m_stream))
    {
        return;
    }

    //异步执行推理
    m_context->enqueue(batchSize, m_buffers, m_stream, nullptr);

    //内存空间复制（推理结果内存空间复制到output内存空间），不确保cudaMemcpyAsync函数返回时已经启动了复制动作
    if (cudaSuccess != cudaMemcpyAsync(output_data, m_buffers[1], output_size, cudaMemcpyDeviceToHost, m_stream))
    {
        return;
    }

    //等待流线程处理完成
    cudaStreamSynchronize(m_stream);
}


