#ifndef SOFTCAMERA_H
#define SOFTCAMERA_H

#include <QTimer>
#include "ImageSrcDev.h"

class SoftCamera : public ImageSrcDev
{
    Q_OBJECT
public:
    SoftCamera(int id, EngineProject *project);

    void onStarted() override;  //启停函数
    void onStopped() override;  //启停函数
    void cleanup() override;    //清除函数

private slots:
    void setCameraNameAsync(const QString &val, const QString &old);
    void setImageSrcPathAsync(const QString &val, const QString &old, const bool &trusted);
    void setIntervalAsync(const int val, const int old);
    void onTimeout();

private:
    // 牌号切换会设置相机参数，虚拟相机如果没有这些属性，IOP将会切换失败，无法用这些图像来训练
    // 相机曝光增益等参数，已经在父类中定义
    QTimer      *m_timer;           //虚拟相机发送图片的定时器
    Attribute   m_imageSrcPath;     //虚拟相机图片的目录
    QStringList m_imageList;        //虚拟相机图像文件列表
    int         m_imageReadIndex;   //虚拟相机读取的图像位置

    bool        m_bStartStage;      //启动阶段标志
    Attribute   m_startDelay;       //定时器启动延时
    Attribute   m_imageInterval;    //虚拟相机图片发送间隔
    Attribute   m_isLoop;           //是否循环发送图片
};

#endif // SOFTCAMERA_H
