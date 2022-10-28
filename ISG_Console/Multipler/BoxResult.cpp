#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QDebug>
#include <QSharedMemory>
#include <QTranslator>
#include "BoxResult.h"
#include "BaseLib/EngineProject.h"
#include "IOCtrl/PCPUdpCtrl.h"
#include "Sensor/BarCoder.h"

CBoxResult::CBoxResult(EngineProject *project):
    AbstractObject(project),
    m_useBarCoder(false),
    m_WaiguanFirstTime(0)
{
}

bool CBoxResult::init()
{
    if (project()->boxQueueLen() == 2) {
        m_queueLen = 2;
    } else {
        m_queueLen = 3;
    }
    m_minElapsed = project()->boxMinElapsed();

    //订阅每个图片的检测完成信号
    for (auto &item : project()->imageMultiplers()) {
        connect(item, SIGNAL(imageResultSignal(const OBJECT_IMAGE_RESULT &)),
                this, SLOT(onImageResult(const OBJECT_IMAGE_RESULT &)));
    }

    //订阅读码信息
    if (BarCoder::getInstance() != NULL) {
        m_useBarCoder = true;
        connect(BarCoder::getInstance(), SIGNAL(receiveCoder(const QByteArray)),
                this, SLOT(onBarCode(const QByteArray)));
    } else {
        m_useBarCoder = false;
    }

    moveToThread(&m_thread);
    m_thread.setObjectName("ObjectResult");
    m_thread.start();
    return true;
}

void CBoxResult::cleanup()
{
    m_thread.quit();
    m_thread.wait();
}

void CBoxResult::preInfer(IMAGE_DATA &imageData)
{
    //采用DirectConnection调用，preInferAsync在调用者线程被调用，需要引入锁
    //如果采用BlockingQueuedConnection，则不需要锁
    QMetaObject::invokeMethod(this, "preInferAsync", Qt::BlockingQueuedConnection, Q_ARG(IMAGE_DATA &, imageData));
}

void CBoxResult::preInferAsync(IMAGE_DATA &imageData)
{
    qint64 currTime = QDateTime::currentMSecsSinceEpoch();

    if (imageData.cameraId == 1) {  //成形图片
        enqueue(currTime);
        imageData.objectId = m_box_queue[0].objectId;
        imageData.bReportOrignal = m_box_queue[0].bReportOrignal;
        imageData.bReportBinning = m_box_queue[0].bReportBinning;
        m_box_queue[0].imageCameraIdHash.insert(imageData.imageId, imageData.cameraId);

//        qInfo() << QString("box shape, cameraId = %1").arg(imageData.cameraId);
    } else if ((imageData.cameraId == 2) || (imageData.cameraId == 3)) { //堆垛图片
        if (!m_box_queue.isEmpty()) {
            imageData.objectId = m_box_queue[0].objectId;
            imageData.bReportOrignal = m_box_queue[0].bReportOrignal;
            imageData.bReportBinning = m_box_queue[0].bReportBinning;
            m_box_queue[0].imageCameraIdHash.insert(imageData.imageId, imageData.cameraId);
        } else {
            imageData.objectId = -1;
            imageData.bReportOrignal = false;
            imageData.bReportBinning = false;
            qInfo() << "can not find box from queue. " << "cameraId: " << imageData.cameraId;
        }
//        qInfo() << QString("box heap, cameraId = %1").arg(imageData.cameraId);
    } else { //外观图片
//        qInfo() << QString("box waiguan, cameraId = %1 Now=%2").arg(imageData.cameraId).arg(QDateTime::currentMSecsSinceEpoch());

        if (currTime - m_WaiguanFirstTime >= 3000)  //箱外观图片成组管理
        {
            m_isWaiguanFirst = true;
            m_WaiguanFirstTime = currTime;

//            qInfo() << QString("waiguan First");
        }
        else { //同一箱烟的后续外观图片
            m_isWaiguanFirst = false;
//            qInfo() << QString("waiguan next");
        }

        if ((m_useBarCoder) && (BarCoder::getInstance()->isConnected())) {
            //读码器读码槽deQueue
        } else if (m_isWaiguanFirst) { //未安装读码器
            dequeue(QByteArray());
        }

        if (m_boxFoundFromQueue)   //找到烟箱数据结构
        {
            imageData.objectId = m_boxPoped.objectId;
            imageData.bReportOrignal = m_boxPoped.bReportOrignal;
            imageData.bReportBinning = m_boxPoped.bReportBinning;
            m_boxPoped.imageCameraIdHash.insert(imageData.imageId, imageData.cameraId);
        } else {
            imageData.objectId = -1;
            imageData.bReportOrignal = false;
            imageData.bReportBinning = false;
        }
    }
}

void CBoxResult::enqueue(qint64 currTime)
{
    BoxObject_S box;

    box.objectId = currTime;
    box.bReportOrignal = orginalReport();
    box.bReportBinning = binningReport();
    box.generateTime = currTime;
    box.result = true;
    m_box_queue.prepend(box);
    if (m_box_queue.count() > m_queueLen) {
        m_box_queue.removeLast();
    }
    m_detectTotalCount++;
}

bool CBoxResult::dequeue(const QByteArray barCode)
{
    m_boxFoundFromQueue = false;
    if (m_box_queue.isEmpty()) {
        return false;
    }
    BoxObject_S &box = m_box_queue.last();

//    qInfo () << QString("genTime: %1 min: %2, Now:%3").arg(box.generateTime).arg(m_minElapsed).arg(QDateTime::currentMSecsSinceEpoch());

    if ((uint64_t)QDateTime::currentMSecsSinceEpoch() >= box.generateTime + m_minElapsed) { // 烟箱读码发生时，烟箱数据至少已存停留时间
        m_boxFoundFromQueue = true;
        m_boxPoped = box;
        m_box_queue.removeLast();
        m_boxPoped.barCode = barCode;
        sendBarCodeInfoToPolicyStore();
        return true;
    }

    return false;
}

void CBoxResult::onBarCode(const QByteArray barCode)
{
    qInfo() << "receive bar: " << barCode;
    dequeue(barCode);
}

BoxObject_S *CBoxResult::findBox(const uint64_t objectId)
{
    for (int i = 0; i < m_box_queue.size(); ++i)
    {
        if (m_box_queue[i].objectId == objectId) {
            return &m_box_queue[i];
        }
    }

    if (m_boxPoped.objectId == objectId) {
        return &m_boxPoped;
    }

    return nullptr;
}

int CBoxResult::findCameraId(BoxObject_S *boxPtr, const uint64_t imageId)
{
    const QHash<uint64_t, uint32_t>  &map = boxPtr->imageCameraIdHash;
    QHash<uint64_t, uint32_t>::const_iterator iter = map.begin();

    while (iter != map.end())
    {
        if (imageId == iter.key())
        {
            return iter.value();
        }
        ++ iter;
    }

    return -1;
}

void CBoxResult::onImageResult(const OBJECT_IMAGE_RESULT &objectImageResult)
{
    BoxObject_S *boxPtr;
    int cameraId;

    boxPtr = findBox(objectImageResult.objectId);
    if (boxPtr == nullptr) {
        return;
    }

    cameraId = findCameraId(boxPtr, objectImageResult.imageId);
    if (cameraId == -1) {
        return;
    }

    boxPtr->imageResultHash.insert(objectImageResult.imageId, objectImageResult.result);
    if (objectImageResult.result == RESULT_NG) {
        if (boxPtr->result) {
            boxPtr->result = false;
            m_detectNgCount++;
        }

        if (PCPUdpCtrl::getInstance()) {
            if ((cameraId >= 1) || (cameraId <= 3)) {
                qInfo().noquote() << QString("send stop machine1.");
                PCPUdpCtrl::getInstance()->sendData(PCPUdpCtrl::UdpCmd_Box_Stop_Machine1, QByteArray());
            } else {
                qInfo().noquote() << QString("send stop machine2.");
                PCPUdpCtrl::getInstance()->sendData(PCPUdpCtrl::UdpCmd_Box_Stop_Machine2, QByteArray());
            }
        }
    } //发现错误
}

void CBoxResult::sendBarCodeInfoToPolicyStore()
{
    if (!m_boxPoped.barCode.isEmpty()) {
        project()->tcpImageReport()->sendBarCodeToPolicyStore(m_boxPoped.objectId, m_boxPoped.barCode);
    }
}
