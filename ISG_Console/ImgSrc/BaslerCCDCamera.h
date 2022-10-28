#ifndef BASLERCCDCAMERA_H
#define BASLERCCDCAMERA_H

#include "ImgSrc/ImageSrcDev.h"
#include <pylon/PylonIncludes.h>
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
#include <pylon/BaslerUniversalInstantCamera.h>
#endif
#include <QMutex>
#include <QTimer>
#include <QThread>
#include <QDebug>

using namespace Pylon;
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
using namespace Basler_UniversalCameraParams;
#endif
using namespace GenApi;
using namespace GenICam;

enum MyEvents
{
    eMyFrameStartEvent,     ///< 帧开始事件.
    eMyExposureEndEvent,    ///< 曝光结束事件.
};

#if (defined USING_CAMERA_BASLER)
class BaslerCCDCamera : public ImageSrcDev, CImageEventHandler, CConfigurationEventHandler
#endif
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
class BaslerCCDCamera : public ImageSrcDev, CImageEventHandler, CConfigurationEventHandler, CBaslerUniversalCameraEventHandler
#endif
{
    Q_OBJECT

public:
    BaslerCCDCamera(int id, EngineProject *project);

    //重写基类的虚函数
    void onStarted() override;  // 启停函数
    void onStopped() override;  // 启停函数
    void cleanup() override;

public:
    void OnImageGrabbed(CInstantCamera &camera, const CGrabResultPtr &ptrGrabResult); //图片处理回调函数
    void OnImagesSkipped(CInstantCamera &camera, size_t countOfSkippedImages);        //图片skipped回调函数
    void OnGrabError(CInstantCamera& camera, const char *errorMessage);               //图片Grab错误回调函数
    void OnCameraDeviceRemoved(CInstantCamera &camera);                               //相机离线回调函数
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
    void OnCameraEvent( CBaslerUniversalInstantCamera& camera, intptr_t userProvidedId, GenApi::INode* pNode); //相机事件回调函数
#endif

private:
    bool findAndInitCamera(const QString &serialNumber); //查找相机
    bool cameraStartGrab(void); // 开始采集
    bool cameraStopGrab(void);  // 停止采集
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
    int64_t getTimestamp();     // 获取相机时间戳，针对gige相机，1个tick对应8ns，具体见Basler手册 https://zh.docs.baslerweb.com/timestamp
#endif

private slots:
    void onFindTimerTimeout();         // 相机查找定时器超时函数
    void onReconnectTimerTimeout();    // 相机重连定时器超时函数
    void onHeartbeatTimerTimeout();    // 相机重连定时器超时函数

protected:
    bool onSerialNumberChange(const QString &val, const QString &old, const bool &trusted) override;
    bool onGainRawChange(const double &val, const double &old, const bool &trusted) override;
    bool onExposeTimeChange(const double &val, const double &old, const bool &trusted) override;
    bool setTrggerMode() override;      // 设置相机拍照触发模式，目前固定设置为硬件触发

private:
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
    int64_t m_FrameStartTimestamp; //帧开始捕获时，相机的tick
    CBaslerUniversalInstantCamera *m_pInstantCamera;    ///< 当前相机
#endif
#if (defined USING_CAMERA_BASLER)
    CInstantCamera *m_pInstantCamera;                   ///< 当前相机
#endif
    CImageFormatConverter   m_ImgConverter;             ///< 相机图像数据转换对象
    QImage::Format          m_imageFormat;              ///< 图像格式
    CDeviceInfo             m_DevInfo;                  ///< 相机信息
    QTimer                  *m_findCameraTimer;         ///< 启动阶段未找到相机，启动定时器寻找对应的相机
    QTimer                  *m_heartbeatTimer;          ///< PC侧停止网卡，basler相机未通知相机离线，增加相机在线定时检测
    QTimer                  *m_ReconnectCameraTimer;    ///< 相机离线后，尝试相机重连定时器
    PylonAutoInitTerm       m_autoInitTerm;             ///< 相机对象构建和销毁时，通过本成员变量的创建和销毁，调用PylonInitialize and PylonTerminate
};

#endif // BASLERCCDCAMERA_H
