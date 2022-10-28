#include <QCoreApplication>
#include <QTcpSocket>
#include <QBuffer>
#include <QDateTime>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QProcess>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
#include "TcpClient.h"
#include "BaseLib/EngineProject.h"
#include "BaseLib/ProtocolStream.h"
#include "ImgSrc/ImageSrcDev.h"
#include "IasAgent/batchInfer.h"
#include "IasAgent/channelInfer.h"

TcpClient::TcpClient(EngineProject *project) :
    EngineObject(project),
    m_socket(new QTcpSocket(this)),
    m_stream(new ProtocolStream(m_socket)),
    m_timer(new QTimer(this)),
    m_statisticReportTimer(new QTimer(this)),
    m_bCleaned(false)
{
    m_timer->setInterval(5000); // 断线5秒后重连
    m_timer->setSingleShot(true);
    m_statisticReportTimer->setInterval(2000);

    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
    connect(m_statisticReportTimer, SIGNAL(timeout()), SLOT(onStatisticReportTimeout()));
    connect(m_socket, SIGNAL(connected()), SLOT(onConnected()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(onDisconnected()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onDisconnected()));
    connect(m_stream, SIGNAL(messageCTRL(POLICY_INFO_MSG_HEADER, QByteArray)),
            SLOT(onMessageCTRL(POLICY_INFO_MSG_HEADER, QByteArray)));

    moveToThread(&m_thread);
    m_thread.setObjectName("TcpClient");
    m_thread.start();
}

void TcpClient::start()
{
    QMetaObject::invokeMethod(this, "onTimeout");
}

void TcpClient::onTimeout()
{
    if (project()->getPolicyStoreOnline()) {
        if (!project()->getRegistaredState()) {
            registerToPolicyStore();
            m_timer->start(); //注册失败重注册
        }
    } else {
        m_socket->connectToHost(project()->policyStore_Ip(), project()->policyStore_port());
    }
}

void TcpClient::onConnected()
{
    qInfo().noquote() << tr("PolicyStore TcpClient: server %1:%2 connected, localport=%3")
                         .arg(m_socket->peerName())
                         .arg(m_socket->peerPort())
                         .arg(m_socket->localPort());
    project()->setPolicyStoreOnline(true);
    registerToPolicyStore();  //向策略中心注册。
    m_timer->start();  //注册失败重注册
    m_statisticReportTimer->start();
}

void TcpClient::onDisconnected()
{
    qInfo().noquote() << tr("PolicyStore TcpClient: tcp disconnected");
    project()->setPolicyStoreOnline(false);
    project()->setRegistaredState(false);

    if (!m_bCleaned) {
        m_socket->disconnectFromHost();
        m_timer->start(); //启动断线重连
        m_statisticReportTimer->stop();
    }
}

void TcpClient::cleanup()
{
    m_bCleaned = true;

    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_statisticReportTimer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_stream, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_socket, "deleteLater", Qt::BlockingQueuedConnection);

    m_thread.quit();
    m_thread.wait();
}

void TcpClient::onMessageCTRL(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body)
{
    qInfo().noquote() << tr("TcpClient receive %1: json data : %2").arg(head.command).arg(body.data());

    QJsonObject ackBody;
    QJsonParseError jsonError;
    QJsonDocument jsonDoc(QJsonDocument::fromJson(body, &jsonError));
    QJsonObject jsonObj = jsonDoc.object();

    // 判断json是否解析成功
    // 注意：个别指令的包体可能为空
    if ((jsonError.error != QJsonParseError::NoError) && head.len) {
        qInfo() <<"PolicyStore TcpClient: json error :"<< jsonError.errorString();
        ackBody.insert("code", CTRL_ERROR);
        ackBody.insert("message", "Bad Request");

        respondToPolicyStore(head, ackBody);
        return;
    }

    switch (head.command) {
        case PsCmd_ACK: //注册响应消息
            if (m_registerMsgTransId == head.tranId) {
                if (jsonObj.value("code").toInt() != CTRL_SUCCESS ) {
                    return;
                }
                project()->tcpImageReport()->establisthTcpReport(jsonObj.value("media_ip").toString(), jsonObj.value("media_port").toInt());

                //ISG注册成功
                project()->setRegistaredState(true);
                qInfo() << "ISG registered success";
            }
            break;

        case PsCmd_ConfigSet: //策略中心参数设置
            if (QDateTime::currentMSecsSinceEpoch() - head.timeStamp > 10000) {
                qInfo().noquote() << "Receive PolicyStore message time more than 10s";
                return;
            }
            setParam(head, jsonObj);
            break;

        case PsCmd_IasSet: //算法参数配置
            {
                QString algorithmType = jsonObj.value("iasType").toString();
                int channelId = jsonObj.value("channelId").toInt();
                int op_type = jsonObj.value("operationType").toInt();
                int dt_type = jsonObj.value("dtType").toInt();
                QJsonObject configJson = jsonObj.value("configuration").toObject();
                QJsonObject resultJson;
                bool result = true;

                if (algorithmType == "AI") {
                    channelId = IasJsonParser::getComponentId(configJson); //从configuration提取
                    QHash<int, AICtrl *> hashTable = project()->AICtrls();
                    QHash<int, AICtrl *>::const_iterator iterator = hashTable.find(channelId);
                    if (iterator != hashTable.end()) {
                        QMetaObject::invokeMethod(iterator.value()->getBatchInfer(),
                                                  "onOperateInfer",
                                                  Qt::BlockingQueuedConnection,
                                                  Q_RETURN_ARG(bool, result),
                                                  Q_ARG(int, op_type),
                                                  Q_ARG(const QJsonObject &, configJson),
                                                  Q_ARG(QJsonObject &, resultJson));
                    }
                }

                if (algorithmType == "DT") {
                    QHash<int, ChannelInfer *> hashTable = project()->channelInfers();
                    QHash<int, ChannelInfer *>::const_iterator iterator = hashTable.find(channelId);
                    if (iterator != hashTable.end()) {
                        QMetaObject::invokeMethod(iterator.value(),
                                                  "onOperateInfer",
                                                  Qt::BlockingQueuedConnection,
                                                  Q_RETURN_ARG(bool, result),
                                                  Q_ARG(int, op_type),
                                                  Q_ARG(int, dt_type),
                                                  Q_ARG(const QJsonObject &, configJson),
                                                  Q_ARG(QJsonObject &, resultJson));
                    }
                }

                if (result) {
                    ackBody.insert("result", resultJson);
                    ackBody.insert("code", CTRL_SUCCESS);
                    ackBody.insert("message", "Success");
                } else {
                    ackBody.insert("code", CTRL_ERROR);
                    ackBody.insert("message", "Error");
                }
                respondToPolicyStore(head, ackBody);
            }
            break;

        case PsCmd_AlarmLedSet:  //设置告警灯信息
        case PsCmd_ContinueKickStart:  //发送连踢开始、结束
            emit sendDataToRealAlarm(head, body);
            break;

        case PsCmd_SetWorkMode: //设置运行模式：点检模式、工作模式
            project()->setAttr("enabled", false, true);

            if (jsonObj.value("work_mode").toInt() == RunType_Spot) {
                project()->setRunType(RunType_Spot);
            } else if (jsonObj.value("work_mode").toInt() == RunType_Infer) {
                project()->setRunType(RunType_Infer);
            }
            ackBody.insert("code", CTRL_SUCCESS);
            ackBody.insert("message", "Success");
            respondToPolicyStore(head, ackBody);
            break;

        case PsCmd_SpotEnable:  //点检启停
            if (project()->getRunType() == RunType_Spot) {
                ImageSrcDev::s_spotDetectimageCount = 0;
                if (jsonObj.value("isStart").toBool()) { //启动点检
                    ImageSrcDev::s_spotDetectWaitFinishCount = jsonObj.value("channelId").toArray().size();
                    qInfo() << QString("s_spotDetectWaitFinishCount = %1").arg(ImageSrcDev::s_spotDetectWaitFinishCount);
                    m_detectStatus = ON_DETECT;
                    for (auto item:jsonObj.value("channelId").toArray()) {
                        QString name = QString("virtual-%1").arg(item.toInt());
                        QString newPath = QString("%1/%2/%3/%4")
                                .arg(QCoreApplication::applicationDirPath() + "/spot")
                                .arg(jsonObj.value("brandId").toInt())
                                .arg(item.toInt())
                                .arg(jsonObj.value("type").toInt() ? "ok" : "ng");

                        if (project()->virtualCameras().contains(name)) {
                            project()->virtualCameras().find(name).value()->setAttr("image_path", newPath);
                        }
                    }
                    project()->setAttr("enabled", true, true);
                } else { //停止点检
                    m_detectStatus = FINISH_DETECT;
                }
                ackBody.insert("code", CTRL_SUCCESS);
                ackBody.insert("message", "Success");
            } else {
                ackBody.insert("code", CTRL_ERROR);
                ackBody.insert("message", "Error");
            }
            respondToPolicyStore(head, ackBody);
            break;

        case PsCmd_InquireSpotDetectStatus: //查询点检状态
            ackBody.insert("code", CTRL_SUCCESS);
            ackBody.insert("message", "Success");
            qInfo() << QString("s_spotDetectWaitFinishCount = %1").arg(ImageSrcDev::s_spotDetectWaitFinishCount);
            if (ImageSrcDev::s_spotDetectWaitFinishCount == 0) {
                m_detectStatus = FINISH_DETECT;
            }

            ackBody.insert("status", m_detectStatus);
            ackBody.insert("total_number", ImageSrcDev::s_spotDetectimageCount);
            respondToPolicyStore(head, ackBody);
            break;

        case PsCmd_Enable:
            if (jsonObj.value("isStart").toBool()) { //启动
                project()->setAttr("enabled", true);
                if (project()->getRunType() == RunType_Spot) {
                    m_detectStatus = ON_DETECT;
                }
            } else {
                project()->setAttr("enabled", false);
                if (project()->getRunType() == RunType_Spot) {
                    m_detectStatus = STOP_DETECT;
                }
            }
            ackBody.insert("code", CTRL_SUCCESS);
            ackBody.insert("message", "Success");
            respondToPolicyStore(head, ackBody);
            break;

        case PsCmd_InquireVersionInfo: //版本信息查询
            {
                QJsonObject devInfo;
                QJsonArray devInfoArray;

                devInfo.insert("module_name", "ISG_Console");
                devInfo.insert("module_version", qApp->applicationVersion());
                devInfo.insert("publish_date", qApp->organizationName());
                devInfoArray.append(devInfo);

                //IO单板版本信息查询
                QStringList pcpVersion = PCPUdpCtrl::getInstance()->getPcpVersionInfo();
                if (pcpVersion.size() >= 3) {
                    devInfo.insert("module_name", pcpVersion.at(0));
                    devInfo.insert("module_version", pcpVersion.at(1));
                    devInfo.insert("publish_date", pcpVersion.at(2));
                    devInfoArray.append(devInfo);
                }

                ackBody.insert("code", CTRL_SUCCESS);
                ackBody.insert("message", "Success");
                ackBody.insert("moduleInfos", devInfoArray);
                respondToPolicyStore(head, ackBody);
            }
            break;

        case PsCmd_CHANGE_BRANDID:
            if (!jsonObj.value("brandId").isNull()) {
                project()->setAttr("brand_id", jsonObj.value("brandId").toInt());
                project()->setAttr("save", true);
                QThread::msleep(100);
                qDebug() << "process id = " << getpid();
                QProcess ::execute(QString("kill -9 %1").arg(getpid()));
            }
            break;

        case PsCmd_REBOOT:
            QThread::msleep(100);
            qDebug() << "process id = " << getpid();
            QProcess ::execute(QString("kill -9 %1").arg(getpid()));
            break;

        default:
            qInfo() <<"PolicyStore TcpClient Error: cmd is not defined to process: "<< head.command;
            ackBody.insert("code", CTRL_ERROR);
            ackBody.insert("message", "Bad Request");

            respondToPolicyStore(head, ackBody);
            break;
    }
}

void TcpClient::registerToPolicyStore()
{
    QJsonObject Body;
    QJsonArray jsonArray;

    Body.insert("rootAddress", QCoreApplication::applicationDirPath());

    for (auto &ptr : project()->AICtrls()) {
        int channelId = ptr->getChannelId();
        int checkSum = ptr->getBatchInfer()->jsonChecksum();

        QJsonObject crcJson;

        crcJson.insert("algorithm_type", IAS_AI);
        crcJson.insert("channel_id", channelId);
        crcJson.insert("check_sum", checkSum);

        jsonArray.append(crcJson);
    }

    for (auto &ptr : project()->channelInfers())
    {
        int channelId = ptr->getChannelId();
        int checkSum = ptr->jsonChecksum() ;

        QJsonObject crcJson;

        crcJson.insert("algorithm_type", IAS_DT);
        crcJson.insert("channel_id", channelId);
        crcJson.insert("check_sum", checkSum);

        jsonArray.append(crcJson);
    }

    Body.insert("brandId", project()->brandId());
    Body.insert("algorithmInfo", jsonArray);

    QJsonDocument doc = QJsonDocument(Body);
    QByteArray array = doc.toJson(QJsonDocument::Compact);

    POLICY_INFO_MSG_HEADER wcpHead;
    wcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
    m_registerMsgTransId = wcpHead.tranId;
    wcpHead.timeStamp = wcpHead.tranId;
    wcpHead.processId = 0;
    wcpHead.processType = PROCESS_TYPE_ISG;
    wcpHead.command = PsCmd_RegisterToPolicy;
    wcpHead.version = VERSION;
    wcpHead.len = array.size();

    qInfo() << "ISG send registered message:" << Body;
    QByteArray msg;
    msg.append((char *)&wcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    msg.append(array);

    m_stream->sendMessage(msg);
}

void TcpClient::setParam(const POLICY_INFO_MSG_HEADER &head, QJsonObject &jsonObj)
{
    ObjectPtr ptr;
    QJsonObject failItem;

    bool bNeedSave = false;
    QJsonObject::Iterator iter = jsonObj.find("save");
    if (iter != jsonObj.end()) {
        bNeedSave = (*iter).toBool();
        jsonObj.erase(iter);
    }

    iter = jsonObj.find("camera_param");
    if (iter != jsonObj.end()) {
        auto camera = (*iter).toObject();
        for (auto param = camera.begin(); param != camera.end(); param++) {
            QJsonArray failCamreaArr;
            QString cameraObjName = param.key();
            ptr = project()->findObject(cameraObjName);
            QJsonObject cameraParam = param.value().toObject();
            for (auto it = cameraParam.begin(); it != cameraParam.end(); it++) {
                QString paramName = it.key();
                QJsonValue value = it.value();
                if (ptr && ptr->setAttr(paramName, value)) {
                    continue;
                }
                failCamreaArr.append(paramName);
            }
            if (failCamreaArr.size() > 0) {
                failItem.insert(cameraObjName, QJsonValue(failCamreaArr));
            }
        }
        jsonObj.erase(iter);
    }

    if (jsonObj.contains("alarm_param")) {
        QJsonDocument doc = QJsonDocument(jsonObj.find("alarm_param").value().toObject());
        QByteArray array = doc.toJson(QJsonDocument::Compact);
        emit sendDataToRealAlarm(head, array);
    }

    iter = jsonObj.find("executor_param");
    if (iter != jsonObj.end()) {
        auto executor = (*iter).toObject();
        for (auto param = executor.begin(); param != executor.end(); param++) {
            QJsonArray failexecutorArr;
            QString executorObjName = param.key();
            ptr = project()->findObject(executorObjName);
            QJsonObject cameraParam = param.value().toObject();
            for (auto it = cameraParam.begin(); it != cameraParam.end(); it++) {
                QString paramName = it.key();
                QJsonValue value = it.value();
                if (paramName == "kick_delay") {
                    if (PCPUdpCtrl::getInstance()) {
                        uint16_t delay = value.toInt();
                        QByteArray buffer;

                        buffer.append((char *)&delay, sizeof(uint16_t));
                        PCPUdpCtrl::getInstance()->sendData(PCPUdpCtrl::UdpCmd_SetKickDelay, buffer);
                        continue;
                    }
                } else {
                    if (ptr && ptr->setAttr(paramName, value)) {
                        continue;
                    }
                }
                failexecutorArr.append(paramName);
            }
            if (failexecutorArr.size() > 0) {
                failItem.insert(executorObjName, QJsonValue(failexecutorArr));
            }
        }
        jsonObj.erase(iter);
    }

    iter = jsonObj.find("global");
    if (iter != jsonObj.end()) {
        QJsonArray failGlobalArr;
        auto config = (*iter).toObject();
        for (auto it = config.begin(); it != config.end(); it++) {
            QString paramName = it.key();
            QJsonValue value = it.value();
            if (project()->setAttr(paramName, value)) {
                continue;
            }
            failGlobalArr.append(paramName);
        }
        if (failGlobalArr.size() > 0) {
            failItem.insert("global", QJsonValue(failGlobalArr));
        }
        jsonObj.erase(iter);
    }

    if (bNeedSave) {
        project()->setAttr("save", true);
    }

    QJsonObject ctrlResult;
    if (failItem.isEmpty()) {
        ctrlResult.insert("code", CTRL_SUCCESS);
        ctrlResult.insert("message", "Success");
    } else {
        ctrlResult.insert("code", CTRL_ERROR);
        ctrlResult.insert("message", "Bad Request");
        ctrlResult.insert("which", failItem);
    }

    respondToPolicyStore(head, ctrlResult);
}

void TcpClient::respondToPolicyStore(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj)
{
    POLICY_INFO_MSG_HEADER wcpHead;

    wcpHead.tranId = head.tranId;
    wcpHead.command = PsCmd_ACK;

    sendToPolicyStore(wcpHead, jsonObj);
}

void TcpClient::sendToPolicyStore(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj)
{
    QMetaObject::invokeMethod(this, "sendToPolicyStoreAsync",
                              Q_ARG(POLICY_INFO_MSG_HEADER, head),
                              Q_ARG(QJsonObject, jsonObj));
}

void TcpClient::sendToPolicyStoreAsync(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj)
{
    QJsonDocument doc = QJsonDocument(jsonObj);
    QByteArray array = doc.toJson(QJsonDocument::Compact);

    POLICY_INFO_MSG_HEADER wcpHead;
    wcpHead.tranId = head.tranId;
    wcpHead.timeStamp = QDateTime::currentMSecsSinceEpoch();
    wcpHead.processId = 0;
    wcpHead.processType = PROCESS_TYPE_ISG;
    wcpHead.command = head.command;
    wcpHead.version = VERSION;
    wcpHead.len = array.size();

    QByteArray sendMsg;
    sendMsg.append((char *)&wcpHead, sizeof(POLICY_INFO_MSG_HEADER));
    sendMsg.append(array);
    qDebug().noquote() <<"report status: " << array.data();
    m_stream->sendMessage(sendMsg);
}

void TcpClient::onStatisticReportTimeout()
{
    QJsonObject jsonObject;
    QJsonArray jsonArray;
    QByteArray sendMsg;
    int channelId = 0;
    int TotalCount = 0;
    int NgCount = 0;
    bool bNeedReport = false;

    if (!project()->getRegistaredState()) {
        return;
    }

    //烟包统计信息构建
    project()->objectResult()->getStatistic(TotalCount, NgCount);
    if ((TotalCount > 0) || (NgCount > 0)) {
        bNeedReport = true;
    }

    jsonObject.insert("channel_id", 0);
    jsonObject.insert("total_count", TotalCount);
    jsonObject.insert("ng_count", NgCount);
    jsonArray.append(jsonObject);

    //烟包统计信息构建
    for (auto &item : project()->imageMultiplers()) {
        channelId = item->id();
        item->getStatistic(TotalCount, NgCount);
        if ((TotalCount > 0) || (NgCount > 0)) {
            bNeedReport = true;
        }

        jsonObject.insert("channel_id", channelId);
        jsonObject.insert("total_count", TotalCount);
        jsonObject.insert("ng_count", NgCount);
        jsonArray.append(jsonObject);
    }

    if (bNeedReport)
    {
        //统计信息消息发送
        POLICY_INFO_MSG_HEADER wcpHead;
        QByteArray byteArray = QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);

        wcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
        wcpHead.timeStamp = wcpHead.tranId;
        wcpHead.command = PsCmd_STATISTIC;
        wcpHead.processId = 0;
        wcpHead.processType = PROCESS_TYPE_ISG;
        wcpHead.version = VERSION;
        wcpHead.len = byteArray.size();

        sendMsg.append((char *)&wcpHead, sizeof(POLICY_INFO_MSG_HEADER));
        sendMsg.append(byteArray);
        qDebug().noquote() <<"report statistic: " << byteArray.data();
        m_stream->sendMessage(sendMsg);
    }
}
