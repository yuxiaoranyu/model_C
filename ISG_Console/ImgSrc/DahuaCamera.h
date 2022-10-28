#ifndef DAHUACAMERA_H
#define DAHUACAMERA_H

#include "ImgSrc/ImageSrcDev.h"
#include "IMVApi.h"
#include <QMutex>
#include <QTimer>
#include <QThread>

class DahuaCamera : public ImageSrcDev
{
    Q_OBJECT

public:
    DahuaCamera(int id, EngineProject *project);

    //重写基类的虚函数
    void onStarted() override;  //启停函数
    void onStopped() override;  //启停函数
    void cleanup() override;    //对象析构的资源函数
    void offlineCleanup();   //相机离线，相机资源清理
    IMV_HANDLE getIMV_Handle() { return m_Imv_Handle; }  //相机句柄

private:
    bool findAndInitCamera(const QString &serialNumber);  //查找并初始化相机
    bool cameraConnect(void);  // 连接相机
    bool cameraCloseConnect(void);  // 关闭相机连接
    bool cameraStartGrab(void); // 开始采集
    bool cameraStopGrab(void); // 停止采集

private slots:
    void onFindTimerTimeout();     // 相机查找定时器超时函数

protected:
    bool onSerialNumberChange(const QString &val, const QString &old, const bool &trusted) override;
    bool onGainRawChange(const double &val, const double &old, const bool &trusted) override;
    bool onExposeTimeChange(const double &val, const double &old, const bool &trusted) override;
    bool setTrggerMode() override;
    QString cameraInfoStr();

private:
    IMV_HANDLE  m_Imv_Handle;   // 相机句柄 | camera handle

public:
    QTimer *m_findCameraTimer; // 相机查找定时器
};

inline QString DahuaCamera::cameraInfoStr()
{
    return QString("Dahua %1(%2)").arg(m_name.value().toString()).arg(m_serialNumber.value().toString());
}

#endif // DAHUACAMERA_H
