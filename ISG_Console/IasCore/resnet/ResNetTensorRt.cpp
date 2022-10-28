#include "ResNetTensorRt.h"

ResNetTensorRt::ResNetTensorRt(const std::string modelPath, const int batch_size) :
    TensorRt(modelPath, 3 * RESNET_IMAGE_WIDTH * RESNET_IMAGE_HEIGHT * sizeof(float), RESNET_OUTPUT_SIZE * sizeof(float))
{
    (void)batch_size;//未启用batch推理
    std::cout << "resnet::init" << std::endl;
}

ResNetTensorRt::~ResNetTensorRt()
{
}

void ResNetTensorRt::preprocessing(const cv::Mat &images, float *blob)
{
    cv::Mat resizedImage, floatImage;
    std::vector<float> mean_value{0.485, 0.456, 0.406};
    std::vector<float> std_value{0.229, 0.224, 0.225};

    cv::cvtColor(images, resizedImage, cv::COLOR_BGR2RGB); //转换成RGB
    cv::resize(resizedImage, resizedImage, cv::Size(224, 224)); //缩放成224*224
    resizedImage.convertTo(floatImage, CV_32FC3, 1 / 255.0); //图像转换，转换格式成32位3通道浮点型图片，alpha尺度变换因子，把原矩阵中的每一个元素都乘以1/255
    std::vector<cv::Mat> bgrChannels(3);
    cv::split(floatImage, bgrChannels);
    for (size_t i = 0; i < bgrChannels.size(); i++)
    {
        bgrChannels[i].convertTo(bgrChannels[i], CV_32FC1, 1.0 / std_value[i], (0.0 - mean_value[i]) / std_value[i]);  //图像转换，转换格式成32位1通道浮点型图片
    }

    cv::merge(bgrChannels, floatImage); //合并单通道图片
    cv::Size floatImageSize{floatImage.cols, floatImage.rows};
    std::vector<cv::Mat> chw(floatImage.channels());
    for (int i = 0; i < floatImage.channels(); ++i)
    {
        chw[i] = cv::Mat(floatImageSize, CV_32FC1, blob + i * floatImageSize.width * floatImageSize.height);
    }
    cv::split(floatImage, chw);
}

std::vector<ResNetResult> ResNetTensorRt::infer(const std::vector<cv::Mat> &images)
{
    std::vector<ResNetResult> resnet_data;
    for (size_t i = 0; i < images.size(); i++)
    {
        ResNetResult result;
        if (m_bInitOk)
        {
            memset((void *)m_cudaInputData, 0, sizeof(m_cudaInputData));
            memset((void *)m_cudaOutputData, 0, sizeof(m_cudaOutputData));
            preprocessing(images[i], m_cudaInputData); //图像预处理
            doInference(1, m_cudaInputData, sizeof(m_cudaInputData), m_cudaOutputData, sizeof(m_cudaOutputData));
            int max_num_index = std::max_element(m_cudaOutputData, m_cudaOutputData + RESNET_OUTPUT_SIZE) - m_cudaOutputData;  //max_element: 返回容器中最大值的指针
            result.classId = max_num_index;
        }

        resnet_data.push_back(result);
    }
    return resnet_data;
}

