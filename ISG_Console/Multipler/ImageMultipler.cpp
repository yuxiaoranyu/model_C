#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QDateTime>
#include "ImageMultipler.h"
#include "BaseLib/EngineProject.h"
#include "ImgSrc/ImageSrcDev.h"
#include "IOCtrl/PLCUdpCtrl.h"
#include "IOCtrl/PCPUdpCtrl.h"

CImageMultipler::CImageMultipler(int id, EngineProject *project):
    EngineObject(id, project),
    m_timer(new QTimer(this)),
    m_IasCount(0),
    m_detectTotalCount(0),
    m_detectNgCount(0)
{
    m_timer->setInterval(20); //扫描图片推理是否超时。间隔如果设置较小，系统开销大；间隔如果设置过大，图片超时时间拉大
    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
}

bool CImageMultipler::init()
{
    m_inferTimeout = project()->imageInferTimeout();

    //订阅实体相机和点检相机图片数据，创建m_imageRecord数据，汇总图片检测结果时，需存在m_imageRecord数据
    for (auto &devices : ImageSrcDev::devices()) {
        if (devices->id() == id()) {
            connect(devices, SIGNAL(imageDataSignal(const IMAGE_DATA &)),
                    this, SLOT(onImageData(const IMAGE_DATA &)));
        }
    }

    // 如果配置了开启AI算法或DT+AI算法，订阅imageIasResultSignal的信号
    for (auto &ptr : project()->AICtrls()) {
        if (ptr->getBatchInfer()->getCarmeraIdList().contains(id())) {
            connect(ptr->getBatchInfer(), SIGNAL(imageIasResultSignal(const IMAGE_IAS_RESULT &)),
                    this, SLOT(onImageIasResult(const IMAGE_IAS_RESULT &)));
            m_IasCount ++;
        }
    }
    // 如果配置了开启DT算法或DT+AI算法，订阅imageIasResultSignal的信号
    if (project()->channelInfers().contains(id())) {
        connect(project()->channelInfers().find(id()).value(), SIGNAL(imageIasResultSignal(const IMAGE_IAS_RESULT &)),
                this, SLOT(onImageIasResult(const IMAGE_IAS_RESULT &)));
        m_IasCount ++;
    }

    m_timer->start();
    qInfo().noquote() << tr("ImageMultipler init success id=%1, algorithm=%2")
                         .arg(id())
                         .arg(project()->getAlgorithmType());

    moveToThread(&m_thread);
    m_thread.setObjectName(QString("ImageMulti_%1").arg(id()));
    m_thread.start();

    return true;
}

void CImageMultipler::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void CImageMultipler::onImageData(const IMAGE_DATA &imageData)
{
    if (m_imageResultRecord.contains(imageData.imageId)) {
        return;
    }

    m_detectTotalCount++;
    IMAGE_RESULT_RECORD imageRecord;
    imageRecord.imageData = imageData;
    m_imageResultRecord.insert(imageData.imageId, imageRecord);
}

void CImageMultipler::onImageIasResult(const IMAGE_IAS_RESULT &iasResult)
{
    QString iasTypeString;

    if (iasResult.cameraId != (uint32_t)id()) {
        return;
    }

    if (iasResult.iasType == IAS_DT) {
        iasTypeString = "DT";
    } else {
        iasTypeString = "AI";
    }

    if (m_imageResultRecord.contains(iasResult.imageId)) {
        IMAGE_RESULT_RECORD &imageRecord = m_imageResultRecord.find(iasResult.imageId).value();
        if (imageRecord.jsonData.isEmpty()) {
            imageRecord.jsonData.append("[");
            imageRecord.jsonData.append(iasResult.jsonData);
            imageRecord.jsonData.append("]");
        } else {
            //将已有的json数据的最后一个]去除，将新数据append后，尾部增加一个]
            QByteArray data;
            data.append(",");
            data.append(iasResult.jsonData);
            data.append("]");
            imageRecord.jsonData.insert(imageRecord.jsonData.size() - 1, data, data.size());
        }
        if (imageRecord.result == RESULT_INIT) {
            imageRecord.result = iasResult.result;
        } else {
            if (iasResult.result == RESULT_NG) {
                imageRecord.result = iasResult.result;
            }
        }

        if (iasResult.result == RESULT_NG) { //烟丝除杂产品，提取杂物坐标，通知PLC剔除
            if ((PLCUdpCtrl::getInstance() != NULL) && (PCPUdpCtrl::getInstance() != NULL) && (PCPUdpCtrl::getInstance()->getKickEnable())) {
                QJsonDocument jsonDoc = QJsonDocument::fromJson(iasResult.jsonData);
                QJsonObject   jsonObj = jsonDoc.object();

                if (!jsonObj.value("isg_info").isUndefined()) {
                    QJsonArray  isgArray = jsonObj.value("isg_info").toArray();
                    if (isgArray.count() > 0)
                    {
                        jsonObj = isgArray.at(0).toObject();
                        if (!jsonObj.value("kick_coordinate").isUndefined()) {
                            jsonObj = jsonObj.value("kick_coordinate").toObject();
                            PLCUdpCtrl::getInstance()->writeData(jsonObj.value("x").toInt(), jsonObj.value("y").toInt());
                        }
                    }
                }
            }
        }

        qInfo().noquote() << tr("ImageMultipler: object_id = %1, camera_id = %2, image_id = %3, ias_type = %4, channel_id = %5, result: %6 cost = %7 Now = %8")
                             .arg(iasResult.objectId)
                             .arg(iasResult.cameraId)
                             .arg(iasResult.imageId)
                             .arg(iasTypeString)
                             .arg(iasResult.channelID)
                             .arg(iasResult.result)
                             .arg(QDateTime::currentMSecsSinceEpoch() - imageRecord.imageData.genTimeStamp)
                             .arg(QDateTime::currentMSecsSinceEpoch());

        if (m_IasCount <= 1) {  //只配置一个算法，直接完成图像的检测
            project()->tcpImageReport()->sendImageResultToPolicyStore(imageRecord);
            if (iasResult.result == RESULT_NG ) {
                m_detectNgCount++;
            }

            if (project()->getRunType() == RunType_Infer) {
                OBJECT_IMAGE_RESULT objectImageResult;

                objectImageResult.objectId = imageRecord.imageData.objectId;
                objectImageResult.imageId = imageRecord.imageData.imageId;
                objectImageResult.result = imageRecord.result;
                emit imageResultSignal(objectImageResult);
            }
            m_imageResultRecord.remove(iasResult.imageId);
            return;
        }

        if (!m_imageIasIdRecord.contains(iasResult.imageId)) {
            m_imageIasIdRecord.insert(iasResult.imageId, 1);
        } else {
            uint32_t receivedCount = m_imageIasIdRecord.find(iasResult.imageId).value();
            receivedCount ++;

            if (receivedCount < m_IasCount)
            {
                m_imageIasIdRecord.insert(iasResult.imageId, receivedCount);
            } else {
                //已经全部接收算法的检测结果
                project()->tcpImageReport()->sendImageResultToPolicyStore(imageRecord);

                if (imageRecord.result == RESULT_NG) {
                    m_detectNgCount++;
                }

                if (project()->getRunType() == RunType_Infer) {
                    OBJECT_IMAGE_RESULT imageResult;

                    imageResult.objectId = imageRecord.imageData.objectId;
                    imageResult.imageId = imageRecord.imageData.imageId;
                    imageResult.result = imageRecord.result;
                    emit imageResultSignal(imageResult);
                }

                m_imageResultRecord.remove(iasResult.imageId);
                m_imageIasIdRecord.remove(iasResult.imageId);
            }
        }
    } else {
        qInfo().noquote() << tr("ImageMultipler: object_id = %1, camera_id = %2, image_id = %3, ias_type = %4, channel_id = %5, result: %6, dropped, Now = %7")
                             .arg(iasResult.objectId)
                             .arg(iasResult.cameraId)
                             .arg(iasResult.imageId)
                             .arg(iasTypeString)
                             .arg(iasResult.channelID)
                             .arg(iasResult.result)
                             .arg(QDateTime::currentMSecsSinceEpoch());
    }
}

void CImageMultipler::onTimeout()
{
    auto nowTime = QDateTime::currentMSecsSinceEpoch();

    for (auto &imageRecord : m_imageResultRecord) {
        //图片配置的超时时间如果超过烟包配置的超时时间，图片的结果综合时，有可能出现烟包已超时，将图片超时时间设置为小于烟包超时时间
        //算法检测一般不会出现超时，超时管理，用于回收资源；
        if (nowTime - imageRecord.imageData.genTimeStamp >= m_inferTimeout) {
            qInfo().noquote() << tr("ImageMultipler: objectId = %1, cameraId = %2, image_id = %3, result: %4, time out, Now = %5")
                                 .arg(imageRecord.imageData.objectId)
                                 .arg(imageRecord.imageData.cameraId)
                                 .arg(imageRecord.imageData.imageId)
                                 .arg(imageRecord.result)
                                 .arg(nowTime);

            if (imageRecord.result == RESULT_NG) {
                m_detectNgCount++;
            }
            if (project()->getRunType() == RunType_Infer) {
                OBJECT_IMAGE_RESULT objectImageResult;

                objectImageResult.objectId = imageRecord.imageData.objectId;
                objectImageResult.imageId = imageRecord.imageData.imageId;
                objectImageResult.result = imageRecord.result;

                //超时时，发送的信号，烟包可能已超时，objectResult可能不处理该信号
                emit imageResultSignal(objectImageResult);
            }
            if (imageRecord.jsonData.isEmpty()) {
                imageRecord.jsonData.append("[]");
            }
            project()->tcpImageReport()->sendImageResultToPolicyStore(imageRecord);

            if (m_imageIasIdRecord.contains(imageRecord.imageData.imageId)) {
                m_imageIasIdRecord.remove(imageRecord.imageData.imageId);
            }
            m_imageResultRecord.remove(imageRecord.imageData.imageId);
            return;
        }
    }
}

void CImageMultipler::getStatistic(int &TotalCount, int &NgCount)
{
    TotalCount = m_detectTotalCount;
    NgCount = m_detectNgCount;

    m_detectTotalCount = 0;
    m_detectNgCount = 0;
}
