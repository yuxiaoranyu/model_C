#include "RealAlarm.h"
#include "BaseLib/EngineProject.h"
#include "TcpClient.h"
#include "ImgSrc/ImageSrcDev.h"
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

RealAlarm::RealAlarm(EngineProject *project) :
    EngineObject(project),
    m_timer(new QTimer(this)),
    m_udpIoLedStateCmd(PCPUdpCtrl::UdpCmd_LedGreen),
    m_reportInterval(300),
    m_reportControlCnt(0),
    m_continueKickEnable(0),
    m_continueKickCount(0),
    m_isContinueKickSet(false),
    m_isUdpIoOnline(false),
    m_boxStatus(STATE_NORMAL),
    m_kickAckStatus(STATE_NORMAL)
{
    m_timer->setInterval(2000);
}

bool RealAlarm::init()
{
    connect(project()->tcpClient(), &TcpClient::sendDataToRealAlarm, this, &RealAlarm::onTcpAlarm);
    if (PCPUdpCtrl::getInstance() != NULL) {
        connect(PCPUdpCtrl::getInstance(), &PCPUdpCtrl::sendDataToRealAlarm, this, &RealAlarm::onUdpIoAlarm);
    }

    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
    m_timer->start();

    moveToThread(&m_thread);
    m_thread.setObjectName("RealAlarm");
    m_thread.start();

    return true;
}

void RealAlarm::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "stop", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);

    m_thread.quit();
    m_thread.wait();
}

void RealAlarm::onTcpAlarm(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body)
{
    int led_state;
    uint16_t pcpCmd;

    QJsonParseError jsonError;
    QJsonDocument jsonDoc(QJsonDocument::fromJson(body, &jsonError));
    QJsonObject jsonObj = jsonDoc.object();

    switch (head.command) {
        case PsCmd_ConfigSet: //参数设置
            m_reportInterval = jsonObj.value("report_interval").toInt(300);             //上报间隔/s
            m_continueKickEnable = jsonObj.value("consecutive_kick_enable").toBool();   //连续踢除使能开关:true/false
            m_continueKickCount = jsonObj.value("consecutive_kick_count").toInt(0);     //连踢个数
            m_isContinueKickSet = true;
            sendContinueKickPara(m_continueKickEnable, m_continueKickCount);
            break;

        case PsCmd_AlarmLedSet: //告警灯设置
            led_state = jsonObj.value("led_state").toInt(IOP_LED_STATE_GREEN);
            if (led_state == IOP_LED_STATE_GREEN) {
                m_udpIoLedStateCmd = PCPUdpCtrl::UdpCmd_LedGreen;
            } else {
                m_udpIoLedStateCmd = PCPUdpCtrl::UdpCmd_LedRed;
            }
            if (PCPUdpCtrl::getInstance()) {
                PCPUdpCtrl::getInstance()->sendData(m_udpIoLedStateCmd, QByteArray());
            }
            respondToPolicyStore(head, true);
            break;

        case PsCmd_ContinueKickStart: {  //连踢控制
                bool kickEnable = jsonObj.find("kick").value().toBool(false);
                if (kickEnable) {
                    pcpCmd = PCPUdpCtrl::UdpCmd_ContinueKickStart; //开始连踢
                } else{
                    pcpCmd = PCPUdpCtrl::UdpCmd_ContinueKickBreak; //停止连踢
                }
                if (PCPUdpCtrl::getInstance()) {
                    PCPUdpCtrl::getInstance()->sendData(pcpCmd, QByteArray());
                }
            }
            respondToPolicyStore(head, true);
            break;

        default:
            break;
    }
}

//处理来自UDP IO单板的告警
void RealAlarm::onUdpIoAlarm(const PCP_UDP_MSG_HEAD &head)
{
    switch (head.cmd) {
        case PCPUdpCtrl::UdpCmd_HeardBeat:
            m_kickAckStatus = STATE_NORMAL;
            m_boxStatus = STATE_NORMAL;
            break;

        case PCPUdpCtrl::UdpCmd_KickStateNg:
            m_kickAckStatus = STATE_ABNORMAL;
            heartbeatToPolicyStore(true);  // pcp剔除异常,立即上报给policy store
            qInfo() << "error : io kick state abnormal, report PolicyStore";
            break;

        case PCPUdpCtrl::UdpCmd_BoxFull:
            m_boxStatus = STATE_ABNORMAL; // 满箱异常,立即上报给policy store
            heartbeatToPolicyStore(true);
            qInfo() << "error : io box state abnormal, report PolicyStore";
            break;

        default:
            break;
    }
}

void RealAlarm::onTimeout()
{
    checkUdpIoConnection();
    heartbeatToUdpIo();
    heartbeatToPolicyStore(false);
}

void RealAlarm::checkUdpIoConnection()
{
    if (QDateTime::currentMSecsSinceEpoch() - PCPUdpCtrl::getInstance()->heartbeatReceivedTime() >= 3000) {
        if (m_isUdpIoOnline) {
            m_isUdpIoOnline = false;  //超过3s为离线
            qInfo() << "error : io device offline,report PolicyStore";
        }
    } else {
        if (!m_isUdpIoOnline)
        {
            m_isUdpIoOnline = true;
            if (m_isContinueKickSet) {
                sendContinueKickPara(m_continueKickEnable, m_continueKickCount);
            }
        }
    }
}

void RealAlarm::sendContinueKickPara(uint8_t continueKickEnable, uint8_t continueKickCount)
{
    QByteArray buffer;
    buffer.append(continueKickEnable);
    buffer.append(continueKickCount);

    if (PCPUdpCtrl::getInstance()) {
        PCPUdpCtrl::getInstance()->sendData(PCPUdpCtrl::UdpCmd_ContinueKickParaPush, buffer);
    }
}

void RealAlarm::heartbeatToUdpIo()
{
    uint16_t pcpCmd;

    if (PCPUdpCtrl::getInstance()->getPcpVersionInfo().isEmpty()) {
        pcpCmd = PCPUdpCtrl::UdpCmd_InquireVersionInfo;
    } else {
        pcpCmd = m_udpIoLedStateCmd;
    }

    if (PCPUdpCtrl::getInstance()) {
        PCPUdpCtrl::getInstance()->sendData(pcpCmd, QByteArray());
    }
}

void RealAlarm::respondToPolicyStore(const POLICY_INFO_MSG_HEADER &msgHead, bool isSuccess)
{
    QJsonObject ctrlResult;

    if (isSuccess) {
        ctrlResult.insert("code", CTRL_SUCCESS);
        ctrlResult.insert("message", "Success");
        project()->tcpClient()->respondToPolicyStore(msgHead, ctrlResult);
    } else {
        ctrlResult.insert("code", CTRL_ERROR);
        ctrlResult.insert("message", "error");
        project()->tcpClient()->respondToPolicyStore(msgHead, ctrlResult);
    }
}

void RealAlarm::triggerReport()
{
    QMetaObject::invokeMethod(this, "onTriggerReport");
}

void RealAlarm::onTriggerReport()
{
    heartbeatToPolicyStore(true);
}

void RealAlarm::heartbeatToPolicyStore(bool bNeedReport)
{
    if (!project()->getRegistaredState()) {
        m_udpIoLedStateCmd = PCPUdpCtrl::UdpCmd_LedRed;
        return;
    }

    ++ m_reportControlCnt;
    //判断所有设备是否正常，不正常则每1s上报，正常则按照[定时]上报

    if (!bNeedReport) {
        bNeedReport = (ImageSrcDev::onlineCameraCount() != ImageSrcDev::devices().size());
    }
    if (!bNeedReport) {
        bNeedReport = (!m_isUdpIoOnline);
    }
    if (!bNeedReport) {
        bNeedReport = (m_kickAckStatus == STATE_ABNORMAL);
    }
    if (!bNeedReport) {
        bNeedReport = (m_boxStatus == STATE_ABNORMAL);
    }
    if (!bNeedReport) {
        bNeedReport = (m_reportControlCnt >= m_reportInterval);
    }
    if (bNeedReport) {
        m_reportControlCnt = 0;

        POLICY_INFO_MSG_HEADER wcpHead;
        wcpHead.tranId = QDateTime::currentMSecsSinceEpoch();
        wcpHead.command = PsCmd_Heartbeat;

        QJsonObject ackBody;
        QJsonArray cameraArr;
        QJsonObject cameraObj;
        QJsonObject ioObj;

        for (int i = 0; i < ImageSrcDev::devices().size(); ++i) {
            if (!ImageSrcDev::devices()[i]->isSpotCamera()) {
                bool isOnline = ImageSrcDev::devices()[i]->isOnline();
                cameraObj.insert("id", ImageSrcDev::devices()[i]->id());
                //相机在线状态:0-异常, 1-正常
                cameraObj.insert("state", (int)isOnline);
                cameraArr.push_back(cameraObj);
            }
        }

        //io板运行情况: 0-异常, 1-正常
        ioObj.insert("connect_state", (int)m_isUdpIoOnline);
        //剔除确认光纤工作情况: 0-异常, 1-正常
        ioObj.insert("kick_state", (int)m_kickAckStatus);
        //踢除烟箱状态: 0-异常, 1-正常
        ioObj.insert("box_state", (int)m_boxStatus);

        ackBody.insert("camera", cameraArr);
        ackBody.insert("io", ioObj);
        ackBody.insert("run_state", project()->getAttr("enabled").toBool());

        if(m_kickAckStatus == STATE_ABNORMAL) {
            m_kickAckStatus = STATE_NORMAL;
        }

        project()->tcpClient()->sendToPolicyStore(wcpHead, ackBody);
    }
}
