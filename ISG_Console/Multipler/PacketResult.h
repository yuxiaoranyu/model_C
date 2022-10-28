#ifndef PACKETRESULT_H
#define PACKETRESULT_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QDebug>
#include <QMutex>
#include "BaseLib/EngineObject.h"
#include "Inspector/Infer.h"
#include "ImageMultipler.h"
#include "AbstractObject.h"

struct PacketObject_S {  //检测对象管理，检测对象有多个图像组成
    PacketObject_S() : result(true){}  //结构体的构造函数
    uint64_t objectId;        //烟包ID
    bool     bReportOrignal;  //确定原图和检测结果是否上报IOP
    bool     bReportBinning;  // 确定Binning图和检测结果是否上报IOP
    uint64_t firstImageTime;  //接收首张图片时记录的图像生成时间
    bool     result;          // 检测结果 true：无错误 false：有错误
    QHash<uint64_t, uint32_t>   imageCameraIdHash; //<imageID, cameraId>烟包中的各个图片的相机信息
    QHash<uint64_t, uint16_t>   imageResultHash;   //<imageID, Result>烟包中的各个图片的结果信息
};

class CPacketResult : public AbstractObject
{
    Q_OBJECT

public:
    CPacketResult(EngineProject *project);

    bool init() override;       //初始化函数
    void cleanup() override;    //清除函数
    void preInfer(IMAGE_DATA &imageData) override;  //成包处理
    void getStatistic(int &TotalCount, int &NgCount) override; //重写统计获取函数

private slots:
    void preInferAsync(IMAGE_DATA &imageData); //成包管理，维护objectId
    void onImageResult(const OBJECT_IMAGE_RESULT &objectImageResult); //接收图片结果
    void onTimeout();   //检测对象的超时处理

private:
    void doKickFinal(const PacketObject_S &record);   //推理完成，通知剔除机构

private:
    QThread  m_thread;
    QTimer   *m_timer;

    QHash<uint64_t, PacketObject_S> m_packetHash;     //并发推理的烟包列表
    uint64_t m_maxInterval;         //同一烟包的多个图像间的最大间隔毫秒数
    uint64_t m_inferTimeout;        //烟包推理超时时间，默认为200ms
};

#endif // PACKETRESULT_H
