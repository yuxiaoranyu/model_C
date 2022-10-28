#include <QTcpSocket>
#include <QBuffer>
#include <QDateTime>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
#include "TcpImageReport.h"
#include "BaseLib/EngineProject.h"
#include "BaseLib/ProtocolStream.h"
#include "ImgSrc/ImageSrcDev.h"
#include "IasAgent/batchInfer.h"
#include "IasAgent/channelInfer.h"

TcpImageReport::TcpImageReport(EngineProject *project) :
    EngineObject(project),
    m_hostPort(0),
    m_socket(new QTcpSocket(this)),
    m_stream(new ProtocolStream(m_socket)),
    m_timer(new QTimer(this)),
    m_isOnline(false),
    m_mediaReportType(IMAGE_REPORT_NULL),
    s_reportQueuedCount(0),
    m_bCleaned(false)
{
    m_timer->setInterval(1000);
    m_timer->setSingleShot(true);

    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
    connect(m_socket, SIGNAL(connected()), SLOT(onConnected()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(onDisconnected()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onDisconnected()));
    connect(m_stream, SIGNAL(messageCTRL(POLICY_INFO_MSG_HEADER, QByteArray)),
            SLOT(onMessageCTRL(POLICY_INFO_MSG_HEADER, QByteArray)));

    moveToThread(&m_thread);
    m_thread.setObjectName("TcpImageReport");
    m_thread.start();
}

void TcpImageReport::establisthTcpReport(QString hostName, quint16 hostPort)
{
    m_hostName = hostName;
    m_hostPort = hostPort;

    qInfo().noquote() << tr("TcpImageReport connect: server %1:%2")
                         .arg(m_hostName)
                         .arg(m_hostPort);

    QMetaObject::invokeMethod(m_timer, "start"); //启动定时器
}

void TcpImageReport::onTimeout()
{
    if (!m_isOnline) {
        qInfo().noquote() << tr("TcpImageReport connect: server %1:%2")
                             .arg(m_hostName)
                             .arg(m_hostPort);
        m_socket->connectToHost(m_hostName, m_hostPort);
        m_timer->setInterval(5000); // 断线5秒后重连
        m_timer->start();
    }
}

void TcpImageReport::onConnected()
{
    qInfo().noquote() << tr("TcpImageReport: server %1:%2 connected, localport=%3")
                         .arg(m_socket->peerName())
                         .arg(m_socket->peerPort())
                         .arg(m_socket->localPort());
    m_isOnline = true;
    sendCreateImageReport();
}

void TcpImageReport::onDisconnected()
{
    qInfo().noquote() << tr("TcpImageReport: tcp disconnected");
    m_isOnline = false;
    if (!m_bCleaned) {
        m_socket->disconnectFromHost();
        m_timer->start();
    }
}

void TcpImageReport::cleanup()
{
    m_bCleaned = true;
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_stream, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_socket, "deleteLater", Qt::BlockingQueuedConnection);

    m_thread.quit();
    m_thread.wait();
}

void TcpImageReport::onMessageCTRL(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body)
{
    qInfo().noquote() << tr("TcpImageReport receive %1: json data : %2").arg(head.command).arg(body.data());

    QJsonParseError jsonError;
    QJsonDocument jsonDoc(QJsonDocument::fromJson(body, &jsonError));
    QJsonObject jsonObj = jsonDoc.object();
    QJsonObject ackBody;

    switch (head.command) {
        case PsCmd_ACK:  //PsCmd_ImageReportCreat应答消息
            if (m_createReportMsgTransId == head.tranId) {
                m_mediaReportType = jsonObj.value("mediaType").toInt();
                qInfo() << "ImageReport create success";
            }
            break;

        case PsCmd_ImageReportMediaType: //媒体类别更新消息
            m_mediaReportType = jsonObj.value("mediaType").toInt();
            ackBody.insert("code", CTRL_SUCCESS);
            ackBody.insert("message", "Success");
            sendResponse(head, ackBody);
            break;

        default:
            break;
    }
}

void TcpImageReport::sendCreateImageReport()
{
    POLICY_INFO_MSG_HEADER tcpHead;
    QByteArray sendMsg;

    tcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
    m_createReportMsgTransId = tcpHead.tranId;
    tcpHead.timeStamp = tcpHead.tranId;
    tcpHead.command = PsCmd_ImageReportCreat;
    tcpHead.processId = 0;
    tcpHead.processType = PROCESS_TYPE_ISG;
    tcpHead.version = VERSION;
    tcpHead.len = 0;
//    qDebug() << "sendCreateImageReport tranId" << tcpHead.tranId;

    sendMsg.append((char *)&tcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    m_stream->sendMessage(sendMsg);
}

void TcpImageReport::sendResponse(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj)
{
    POLICY_INFO_MSG_HEADER wcpHead;
    QByteArray sendMsg;
    QJsonDocument doc = QJsonDocument(jsonObj);
    QByteArray array = doc.toJson(QJsonDocument::Compact);

    wcpHead.tranId = head.tranId;
    wcpHead.timeStamp = QDateTime::currentMSecsSinceEpoch();
    wcpHead.processId = 0;
    wcpHead.processType = PROCESS_TYPE_ISG;
    wcpHead.command = PsCmd_ACK;
    wcpHead.version = VERSION;
    wcpHead.len = array.size();

    sendMsg.append((char *)&wcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    sendMsg.append(array);
//    qDebug().noquote() <<"TcpImageReport response message: " << array.data();
    m_stream->sendMessage(sendMsg);
}

void TcpImageReport::sendImageResultToPolicyStore(const IMAGE_RESULT_RECORD &imageRecord)
{
    if (!m_isOnline) {
        return;
    }

    if ((m_mediaReportType < IMAGE_REPORT_ORIGINAL) || (m_mediaReportType > IMAGE_REPORT_BOTH)) {
        return;
    }

    if (imageRecord.imageData.bReportOrignal || imageRecord.imageData.bReportBinning) {
        if (s_reportQueuedCount < MAX_IMG_REPORT_QUEUE_SIZE) {
            s_reportQueuedCount++;
            QMetaObject::invokeMethod(this, "sendImageResultToPolicyStoreAsync", Q_ARG(const IMAGE_RESULT_RECORD &, imageRecord));
        } else {
            qInfo() << "TcpImageReport: image report queue is full";
        }
    }
}

void TcpImageReport::sendImageResultToPolicyStoreAsync(const IMAGE_RESULT_RECORD &imageRecord)
{
    bool binningReport = false;
    bool orignalReport = false;
    bool bBinning = false;

    s_reportQueuedCount--;
    if (((m_mediaReportType == IMAGE_REPORT_ORIGINAL) || (m_mediaReportType == IMAGE_REPORT_BOTH)) && imageRecord.imageData.bReportOrignal) {
        orignalReport = true;
    }

    if (((m_mediaReportType == IMAGE_REPORT_BINNING) || (m_mediaReportType == IMAGE_REPORT_BOTH)) && imageRecord.imageData.bReportBinning) {
        binningReport = true;
    }

    if ((!orignalReport) && (!binningReport)) {
        return;
    }

    if (m_socket->state() != QAbstractSocket::ConnectedState) {     // 网络异常，直接退出发送图片
        return;
    }

    QImage image = imageRecord.imageData.imageData;
    if ((binningReport) && (!orignalReport))
    {
        bBinning = true;
        image = image.scaled(640, 480);
    }
    QByteArray tcpMsg(sizeof(IMAGE_DATA_INFO), Qt::Uninitialized);  // 用Append的方式，把QImage图片数据添加到IMAGE_DATA_INFO后
    {
        QBuffer buffer(&tcpMsg);
        buffer.open(QIODevice::Append);
        image.save(&buffer, "jpg");
        buffer.close();
    }

    // 给IMAGE_DATA_INFO逐一赋值
    IMAGE_DATA_INFO *imageInfo = reinterpret_cast<IMAGE_DATA_INFO *>(tcpMsg.data());
    imageInfo->objectId = imageRecord.imageData.objectId;
    imageInfo->cameraId = imageRecord.imageData.cameraId;
    imageInfo->imageId = imageRecord.imageData.imageId;
    imageInfo->genTimeStamp = imageRecord.imageData.genTimeStamp;
    imageInfo->imageType = image.depth();
    imageInfo->imageWidth = image.width();
    imageInfo->imageHeight = image.height();
    imageInfo->result = imageRecord.result;
    imageInfo->imageSize = tcpMsg.size() - sizeof(IMAGE_DATA_INFO); // 去掉IMAGE_DATA_INFO头即是imageSize
    imageInfo->jsonSize = imageRecord.jsonData.size();
    tcpMsg.append(imageRecord.jsonData);

    // WCP通讯头
    POLICY_INFO_MSG_HEADER tcpHead;
    tcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
    tcpHead.timeStamp = tcpHead.tranId;
    if (bBinning) {
        tcpHead.command = PsCmd_ImageResultReportBinning;
        qInfo() << "TcpImageResultReport: binning image report";
    } else {
        tcpHead.command = PsCmd_ImageResultReport;
        qInfo() << "TcpImageResultReport: orginal image report";
    }
    tcpHead.processId = 0;
    tcpHead.processType = PROCESS_TYPE_ISG;
    tcpHead.version = VERSION;
    tcpHead.len = tcpMsg.size();

    QByteArray sendMsg;
    sendMsg.append((char *)&tcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    sendMsg.append(tcpMsg);
    m_stream->sendMessage(sendMsg);
}

void TcpImageReport::sendImageDataToPolicyStore(const IMAGE_DATA &imageData)
{
    if (!m_isOnline) {
        return;
    }
    if (s_reportQueuedCount < MAX_IMG_REPORT_QUEUE_SIZE) {
        s_reportQueuedCount++;
        QMetaObject::invokeMethod(this, "sendImageDataToPolicyStoreAsync", Q_ARG(const IMAGE_DATA &, imageData));
    } else {
        qInfo() << "TcpImageReport: image report queue is full";
    }
}

void TcpImageReport::sendImageDataToPolicyStoreAsync(const IMAGE_DATA &imageData)
{
    s_reportQueuedCount--;
    if (m_socket->state() != QAbstractSocket::ConnectedState) {     // 网络异常，直接退出发送图片
        return;
    }

    const QImage image = imageData.imageData;
    QByteArray tcpMsg(sizeof(IMAGE_DATA_INFO), Qt::Uninitialized);  // 用Append的方式，把QImage图片数据添加到IMAGE_DATA_INFO后
    {
        QBuffer buffer(&tcpMsg);
        buffer.open(QIODevice::Append);
        image.save(&buffer, "jpg");
        buffer.close();
    }

    // 给IMAGE_DATA_INFO逐一赋值
    IMAGE_DATA_INFO *imageInfo = reinterpret_cast<IMAGE_DATA_INFO *>(tcpMsg.data());
    imageInfo->objectId = 0;
    imageInfo->cameraId = imageData.cameraId;
    imageInfo->imageId = imageData.imageId;
    imageInfo->genTimeStamp = imageData.genTimeStamp;
    imageInfo->imageType = image.depth();
    imageInfo->imageWidth = image.width();
    imageInfo->imageHeight = image.height();
    imageInfo->result = true;
    imageInfo->imageSize = tcpMsg.size() - sizeof(IMAGE_DATA_INFO); // 去掉IMAGE_DATA_INFO头即是imageSize
    imageInfo->jsonSize = 0;

    // WCP通讯头
    POLICY_INFO_MSG_HEADER tcpHead;
    tcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
    tcpHead.timeStamp = tcpHead.tranId;
    tcpHead.command = 5001;
    tcpHead.processId = 0;
    tcpHead.processType = PROCESS_TYPE_ISG;
    tcpHead.version = VERSION;
    tcpHead.len = 0;

    QByteArray sendMsg;
    sendMsg.append((char *)&tcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    sendMsg.append(tcpMsg);
    m_stream->sendMessage(sendMsg);
}

void TcpImageReport::sendBarCodeToPolicyStore(const uint64_t objectId, const QByteArray &barCode)
{
    if (!m_isOnline) {
        return;
    }

    QMetaObject::invokeMethod(this, "sendBarCodeToPolicyStoreAsync", Q_ARG(const uint64_t , objectId), Q_ARG(const QByteArray , barCode));
}

void TcpImageReport::sendBarCodeToPolicyStoreAsync(const uint64_t objectId, const QByteArray &barCode)
{
    (void)objectId;
    (void)barCode;

    //待对齐接口
}
