#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QDebug>
#include <QSharedMemory>
#include <QTranslator>
#include "PacketResult.h"
#include "BaseLib/EngineProject.h"
#include "ImgSrc/ImageSrcDev.h"
#include "IOCtrl/PCPSerialCtrl.h"
#include "IOCtrl/SiemensS7PLC.h"
#include "IOCtrl/PCPUdpCtrl.h"

namespace {
    inline uint64_t distance(uint64_t a, uint64_t b)
    {
        if (a > b) return a - b;
        if (b > a) return b - a;
        return 0;
    }
}

CPacketResult::CPacketResult(EngineProject *project):
    AbstractObject(project),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(20); //扫描烟包推理是否超时。间隔如果设置较小，系统开销大；间隔如果设置过大，超时的烟包超时时间拉大
    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
}

bool CPacketResult::init()
{
    m_maxInterval = project()->imageMaxInterval();
    m_inferTimeout = project()->objectInferTimeout();

    //订阅每个图片的检测完成信号
    for (auto &item : project()->imageMultiplers()) {
        connect(item, SIGNAL(imageResultSignal(const OBJECT_IMAGE_RESULT &)),
                this, SLOT(onImageResult(const OBJECT_IMAGE_RESULT &)));
    }
    m_timer->start();
    moveToThread(&m_thread);
    m_thread.setObjectName("ObjectResult");
    m_thread.start();
    return true;
}

void CPacketResult::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void CPacketResult::preInfer(IMAGE_DATA &imageData)
{
    //采用DirectConnection调用，preInferAsync在调用者线程被调用，需要引入锁
    //如果采用BlockingQueuedConnection，则不需要锁
    QMetaObject::invokeMethod(this, "preInferAsync", Qt::BlockingQueuedConnection, Q_ARG(IMAGE_DATA &, imageData));
}

void CPacketResult::preInferAsync(IMAGE_DATA &imageData)
{
    for (auto &i : m_packetHash) {
        // 在设定的时间间隔内，认为属于同一烟包的图片
        if (distance(imageData.genTimeStamp, i.firstImageTime) <= m_maxInterval) {
            bool bCameraIdExisted = false;
            for (auto camera_id : i.imageCameraIdHash) {
                // 该相机位已经有了图片,另外成一包
                if (camera_id == imageData.cameraId) {
                    qInfo().noquote() << tr("Object generate: Camera-%1 already exists!").arg(camera_id);
                    bCameraIdExisted = true;
                    break;
                }
            }
            if (bCameraIdExisted) {
                break;
            }
            // 该烟包的照片数大于等于相机数(该烟包全部面已经都有了),另外成一包
            if (i.imageCameraIdHash.size() >= ImageSrcDev::realOnlineCameraCount()) {
                qInfo().noquote() << tr("Object generate: image number is more than camera count!");
                break;
            } else {    // 该烟包的照片数少于相机数,插入该照片
                i.imageCameraIdHash.insert(imageData.imageId, imageData.cameraId);
                imageData.objectId = i.objectId;
                imageData.bReportOrignal = i.bReportOrignal;
                imageData.bReportBinning = i.bReportBinning;
                return;
            }
        }
    }
    // 另外成一烟包
    m_detectTotalCount++;
    PacketObject_S record;

    record.bReportOrignal = orginalReport();
    record.bReportBinning = binningReport();
    record.objectId = QDateTime::currentMSecsSinceEpoch();
    record.firstImageTime = imageData.genTimeStamp;
    record.imageCameraIdHash.insert(imageData.imageId, imageData.cameraId);
    imageData.objectId = record.objectId;
    imageData.bReportOrignal = record.bReportOrignal;
    imageData.bReportBinning = record.bReportBinning;

    qInfo().noquote() << tr("Object generate: objectId = %1").arg(record.objectId);
    m_packetHash.insert(record.objectId, record);
}

void CPacketResult::onImageResult(const OBJECT_IMAGE_RESULT &objectImageResult)
{
    // 没有该烟包记录,不做剔除处理
    if (!m_packetHash.contains(objectImageResult.objectId)) {
        return;
    }

    PacketObject_S &objRecord = m_packetHash.find(objectImageResult.objectId).value();
    // 查找到该照片的id,插入result
    if (objRecord.imageCameraIdHash.contains(objectImageResult.imageId)) {
        objRecord.imageResultHash.insert(objectImageResult.imageId, objectImageResult.result);
    }

    // 该烟包的所有相机的result全部到了,判断是否要剔除
    if ((objRecord.imageResultHash.size() >= ImageSrcDev::realOnlineCameraCount())
        || (objRecord.imageResultHash.size() >= objRecord.imageCameraIdHash.size()))
    {
        bool objResult = true;
        for (auto &result : objRecord.imageResultHash) {
            if (result == RESULT_NG) {
                objResult = false;
                break;
            }
        }
        qInfo().noquote() << tr("ObjectResult: objectId = %1, image count = %2, result: %3, cost = %4")
                             .arg(objRecord.objectId)
                             .arg(objRecord.imageCameraIdHash.size())
                             .arg(objResult)
                             .arg(QDateTime::currentMSecsSinceEpoch() - objRecord.firstImageTime);
        objRecord.result = objResult;
        if (!objResult) {
            m_detectNgCount ++;
        }
        doKickFinal(objRecord);
        m_packetHash.remove(objRecord.objectId);
    }
}

///@brief 检测对象的超时处理
void CPacketResult::onTimeout()
{
    uint64_t nowTime = QDateTime::currentMSecsSinceEpoch();
    for (auto &objRecord : m_packetHash) {
        if (nowTime - objRecord.firstImageTime >= m_inferTimeout) {
            if (objRecord.imageResultHash.isEmpty()) {
                qInfo().noquote() << tr("ObjectResult: objectId = %1, image count = %2, result: time out, cost = %3")
                                     .arg(objRecord.objectId)
                                     .arg(objRecord.imageCameraIdHash.size())
                                     .arg(nowTime - objRecord.firstImageTime);
                m_packetHash.remove(objRecord.objectId);
                break;
            }

            bool objResult = true;
            for(auto &result : objRecord.imageResultHash) {
                if (result == RESULT_NG) {
                    objResult = false;
                    break;
                }
            }
            objRecord.result = objResult;
            if (!objResult) {
                m_detectNgCount ++;
            }
            qInfo().noquote() << tr("ObjectResult: objectId = %1, image count = %2, result count = %3, result: %4, cost = %5 time out")
                                 .arg(objRecord.objectId)
                                 .arg(objRecord.imageCameraIdHash.size())
                                 .arg(objRecord.imageResultHash.size())
                                 .arg(objResult)
                                 .arg(QDateTime::currentMSecsSinceEpoch() - objRecord.firstImageTime);

            //如果一包烟的多个图片，成包成两个包了，即使发现错误，也不剔除
            if (objRecord.imageCameraIdHash.size() >= ImageSrcDev::realOnlineCameraCount()) {
                doKickFinal(objRecord);
            }
            m_packetHash.remove(objRecord.objectId);
            break;
        }
    }
}

void CPacketResult::getStatistic(int &TotalCount, int &NgCount)
{
    AbstractObject::getStatistic(TotalCount, NgCount);

    if (PCPUdpCtrl::getInstance()) {
        //使用IO板记录的剔除数
        PCPUdpCtrl::getInstance()->getNGCount(NgCount);
    }
}

void CPacketResult::doKickFinal(const PacketObject_S &record)
{
    if (!record.result) {
        m_detectNgCount ++;
        qInfo().noquote() << QString("object = %1, Now = %2, send kick msg!").arg(record.objectId).arg(QDateTime::currentMSecsSinceEpoch());

        if (PCPSerialCtrl::getInstance()) {
            PCPSerialCtrl::getInstance()->PCP_KickSendData();
#ifdef Q_OS_LINUX
        } else if (SiemensS7PLC::getInstance()) {
            SiemensS7PLC::getInstance()->sendDataToPLC();
#endif
        } else if (PCPUdpCtrl::getInstance()) {
            PCP_UDP_MSG_PacketProduct_Result_Body msgBody;
            QByteArray buff;

            msgBody.genSecond = record.firstImageTime / 1000;
            msgBody.genMillisecond = record.firstImageTime % 1000;
            msgBody.result = 1;
            buff.append((char *)&msgBody, sizeof(PCP_UDP_MSG_PacketProduct_Result_Body));
            PCPUdpCtrl::getInstance()->sendData(PCPUdpCtrl::UdpCmd_Kick, buff);
            if (PCPUdpCtrl::getInstance()->getKickEnable()) {
                if (project()->productType() == PRODUCT_YANSI_STOPMODE) {
                    project()->setAttr("enabled", false);
                    project()->realAlarm()->triggerReport();
                }
            }
        }
    } else {
        qInfo().noquote() << QString("object = %1, Now = %2, result is true").arg(record.objectId).arg(QDateTime::currentMSecsSinceEpoch());
    }
}
