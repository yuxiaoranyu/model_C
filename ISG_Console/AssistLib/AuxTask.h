#ifndef AUXTASK_H
#define AUXTASK_H

#include <QAtomicInt>
#include <QMessageLogContext>
#include <QImage>
#include <QJsonObject>
#include <QThread>
#include <QObject>
#include <opencv2/opencv.hpp>

#define MAX_IMG_SAVE_QUEUE_SIZE     7           // 等待图像保存的最大排队消息个数
#define MAX_LOG_FILE_SIZE           200000000   // 允许的log文件大小，超过200M空间，log信息不再存储
#define MIN_FREE_SPACE              30720       // 最小磁盘剩余空间, M为单位，小于30G空间，需要清除历史图像文件
#define MAX_IMG_CLEAN_QUEUE_SIZE    1           // 等待图像清理的最大排队消息个数
#define MAX_LOG_COUNT               2           // 每2个log输出存一次文件

class AuxTask : public QObject
{
    Q_OBJECT

public:
    AuxTask();
    ~AuxTask();

public:
    static void outPutLog(const QtMsgType type, const QMessageLogContext &context, const QString &msg); // 记录日志

    void saveImage(const QImage &image, const QString &saveFilePath, const QString &saveFileName, const QString &saveFileFormat);   // 保存图像文件
    void saveConfigFile(const QString &jsonFileName, const QJsonObject &jsonFileObj);   // 保存product_params.json文件
    void cleanExpiredLogFiles(const int logFileKeepDays);                               // 清除过期日志文件
    void checkConfigFile(const QString &jsonFileName);      //设备掉电，出现product_params.json文件内容为空的情况。使用back文件进行异常场景问题的解决
    void cleanExpiredImageDirectory(const QString &imagePath, const int imageFileKeepDays); // 清除过期图像路径
    void cleanImageDirectoryForNoSpace(const QString &imageRootPath);                   // 磁盘无空间，清除图像路径
    static cv::Mat QImageToCvMat(const QImage &qImageInput);  //QT图片数据转换成OpenCV的图片数据

private slots:
    void saveImageAsync(const QImage &image, const QString &saveFilePath, const QString &saveFileName, const QString &saveFileFormat);
    void saveConfigFileAsync(const QString &jsonFileName, const QJsonObject &jsonFileObj);
    void cleanExpiredLogFilesAsync(const int logFileKeepDays);
    void cleanExpiredImageDirectoryAsync(const QString &imageRootPath, const int imageFileKeepDays);
    void cleanImageDirectoryForNoSpaceAsync(const QString &imageRootPath);

private:
    QThread    m_thread;            // AuxTask线程
    QAtomicInt s_saveImageCount;    // 等待保存的图像任务数目，对等待保存的图像任务数目进行限制
    QAtomicInt s_cleanImageCount;   // 等待图像清理的任务数目，对等待图像清理的任务数目进行限制

    static QString          s_logString; //待存储日志的字符串
    static QAtomicInt       s_logCount;  //待存储日志的字符串包含的日志数
    static QtMessageHandler s_messageHandler;  // QT定义的日志消息处理函数
};

#endif // AUXTASK_H
