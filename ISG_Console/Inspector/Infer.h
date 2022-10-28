// 推理数据结构

#ifndef INFER_H
#define INFER_H

#include <cstdint>
#include <QImage>
#include <opencv2/opencv.hpp>

#define RESULT_INIT            -1          //结果处于初始化状态
#define RESULT_NG               0          //NG 结果不正确
#define RESULT_OK               1          //OK 结果正确
#define RESULT_TIMEOUT          2          //结果超时

#pragma pack(1)

typedef struct tagIMAGE_DATA
{
    uint64_t objectId;             // 对象ID
    bool     bReportOrignal;       // 原图和检测结果上报IOP
    bool     bReportBinning;       // Binning图和检测结果上报IOP
    uint32_t cameraId;             // 相机标识
    uint64_t imageId;              // 相机的图像ID
    QImage   imageData;            // 图片数据
    cv::Mat  imageDataCV;          // 图片数据
    uint64_t genTimeStamp;         // 图片FrameStart发生时间
} IMAGE_DATA;

typedef struct tagIMAGE_GROUP_DATA {
    uint64_t objectId;                          // 对象ID
    QVector<uint32_t> cameraIdVector;           // 相机标识Vector
    QVector<uint64_t> imageIdVector;            // 图像ID Vector
    QVector<cv::Mat>  imageDataVector;          // 图片数据Vector
    qint64            genTimeStamp;             // 生成时间
} IMAGE_GROUP_DATA;

typedef struct tagIMAGE_IAS_RESULT
{
    uint64_t objectId;              // 对象ID
    uint32_t cameraId;              // 相机标识
    uint64_t imageId;               // 相机的图像ID
    uint32_t iasType;               // 算法类别
    uint32_t channelID;             // 算法通道Id
    int result;                     // 图片结果
    QByteArray jsonData;            // 结果json
} IMAGE_IAS_RESULT;

#pragma pack()

#endif // INFER_H
