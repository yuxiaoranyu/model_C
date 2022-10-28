#include "common.hpp"
#include "utils.h"
#include "preprocess.h"
#include "Yolov5TensorRt.h"
#include <QDebug>

static const int OutputSize = Yolo::MAX_OUTPUT_BBOX_COUNT * sizeof(Yolo::Detection) / sizeof(float) + 1;

Yolov5TensorRt::Yolov5TensorRt(const std::string modelPath, const int batch_size) :
    TensorRt(modelPath, batch_size * 3 * YOLO_IMAGE_WIDTH * YOLO_IMAGE_HEIGHT * sizeof(float), batch_size * OutputSize * sizeof(float)),
    m_batch_size(batch_size)
{
    m_cudaInputData = new float[batch_size * 3 * YOLO_IMAGE_WIDTH * YOLO_IMAGE_HEIGHT];
    m_cudaOutputData = new float[batch_size * OutputSize];

    cudaSetDevice(0);
}

Yolov5TensorRt::~Yolov5TensorRt()
{
    delete[] m_cudaInputData;
    delete[] m_cudaOutputData;
}

std::vector<std::vector<YoloResult>> Yolov5TensorRt::infer(const std::vector<cv::Mat> &images)
{
    int channel1_offset = YOLO_IMAGE_HEIGHT * YOLO_IMAGE_WIDTH;
    int channel2_offset = 2 * YOLO_IMAGE_HEIGHT * YOLO_IMAGE_WIDTH;
    std::cout<<"image size "<<images.size()<<std::endl;
    std::cout<<"m_batch_size "<<m_batch_size<<std::endl;

    //输入的图像数目超过batch_size或TensorRt初始化失败，直接返回
    if (((int)images.size() > m_batch_size) || !m_bInitOk) {
        std::vector<std::vector<YoloResult>> results(images.size());
        for (size_t i = 0; i < images.size(); i++)
        {
            qInfo() << "Yolov5TensorRt infer error!";
            return results;
        }
    }

    auto start = std::chrono::system_clock::now();
    std::vector<cv::Mat> imgs_buffer(m_batch_size);

    memset((void *)m_cudaInputData, 0, sizeof(m_cudaInputData));
    memset((void *)m_cudaOutputData, 0, sizeof(m_cudaOutputData));

    for (size_t i = 0; i < images.size(); i++)
    {
        if (images[i].channels() == 1)
        {
            cv::cvtColor(images[i], imgs_buffer[i], cv::COLOR_GRAY2BGR);
        }
        else if (images[i].channels() == 3)
        {
            imgs_buffer[i] = images[i];
        }
        else if (images[i].channels() == 4)
        {
            cv::cvtColor(images[i], imgs_buffer[i], cv::COLOR_BGRA2BGR);
        }
        else
        {
            qInfo() << "input image format error";
        }

        //图像预处理
        cv::Mat pr_img = preprocess_img(imgs_buffer[i], YOLO_IMAGE_WIDTH, YOLO_IMAGE_HEIGHT);

        int image_offset =  i * 3 * YOLO_IMAGE_HEIGHT * YOLO_IMAGE_WIDTH;
        int idx = 0;
        for (int row = 0; row < YOLO_IMAGE_HEIGHT; ++row)
        {
            uchar *uc_pixel = pr_img.data + row * pr_img.step;
            for (int col = 0; col < YOLO_IMAGE_WIDTH; ++col)
            {
                //输入缓冲区构建
                m_cudaInputData[image_offset + idx] = (float)uc_pixel[2] / 255.0;
                m_cudaInputData[image_offset + idx + channel1_offset] = (float)uc_pixel[1] / 255.0;
                m_cudaInputData[image_offset + idx + channel2_offset] = (float)uc_pixel[0] / 255.0;
                uc_pixel += 3;
                ++idx;
            }
        }
    }

    //TensorRt推理
    doInference(m_batch_size, m_cudaInputData, m_batch_size * 3 * YOLO_IMAGE_WIDTH * YOLO_IMAGE_HEIGHT * sizeof(float) , m_cudaOutputData, m_batch_size * OutputSize * sizeof(float));

    //返回结果处理
    std::vector<std::vector<YoloResult>> results(images.size());
    for (size_t i = 0; i < images.size(); i++)
    {
        std::vector<Yolo::Detection> res;
        nms(res, &m_cudaOutputData[i * OutputSize], 0.5, 0.4); // CONF_THRESH ,NMS_THRESH
        std::vector<YoloResult> result(res.size());
        for (size_t j = 0; j < res.size(); j++)
        {
            result[j].box = get_rect(images[i], res[j].bbox);
            result[j].conf = res[j].conf;
            result[j].classId = res[j].class_id;
        }
        results[i] = result;
    }
    auto end = std::chrono::system_clock::now();
    std::cout << "inference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    return results;
}
