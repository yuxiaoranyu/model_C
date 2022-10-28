#include "PCPUdpCtrl.h"
#include "./IopAgent/RealAlarm.h"
#include "./BaseLib/EngineProject.h"
#include <QUdpSocket>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>

PCPUdpCtrl *PCPUdpCtrl::s_pInstance = NULL;

PCPUdpCtrl::PCPUdpCtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_socket(new QUdpSocket(this)),
    m_udpLocalIp("local_ip", "0.0.0.0", this),
    m_udpLocalPort("local_port", 3000, this),
    m_udpRemoteIp("remote_ip", "0.0.0.0", this),
    m_udpRemotePort("remote_port", 3000, this),
    m_kickEnable("connected", true, this),
    m_kickAbnormalEnable("abnormal_kick", false, this),
    m_heartbeatLastTime(0),
    m_kickCount(0)
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    m_kickEnable.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        // 启动阶段socket未初始化，不能下发剔除使能标志
        if (!trusted) {
            setKickEnabled(value.toBool());
        }
        return true;
    });

    m_kickAbnormalEnable.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        // 启动阶段socket未初始化，不能下发剔除使能标志
        if (!trusted) {
            setKickAbnormalEnabled(value.toBool());
        }
        return true;
    });

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

bool PCPUdpCtrl::init()
{
    m_socket->bind(QHostAddress(m_udpLocalIp.value().toString()), m_udpLocalPort.value().toInt());

    moveToThread(&m_thread);
    m_thread.setObjectName("PCPUdp");
    m_thread.start();

    setKickEnabled(m_kickEnable.value().toBool());
    setKickAbnormalEnabled(m_kickAbnormalEnable.value().toBool());

    return true;
}

void PCPUdpCtrl::cleanup()
{
    onStopped();

    m_thread.quit();
    m_thread.wait();
}

PCPUdpCtrl *PCPUdpCtrl::getInstance()
{
    return s_pInstance;
}

void PCPUdpCtrl::onStarted()
{
    //烟丝除杂产品，上位机启动，通知IO单板开始运行
    QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_RunStart), Q_ARG(const QByteArray &, QByteArray()));
}

void PCPUdpCtrl::onStopped()
{
    //烟丝除杂产品，上位机停止工作，通知IO单板停止运行
    QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_RunStop), Q_ARG(const QByteArray &, QByteArray()));
}

void PCPUdpCtrl::sendData(const uint16_t cmd, const QByteArray &buffer)
{
    if ((cmd == UdpCmd_Kick) && !m_kickEnable.value().toBool()) {
        return;
    }

    QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, cmd), Q_ARG(const QByteArray &, buffer));
}

void PCPUdpCtrl::sendDataAsync(const uint16_t cmd, const QByteArray &buffer)
{
    PCP_UDP_MSG_HEAD msgHead;

    msgHead.magic = PCP_UDP_MAGIC;
    msgHead.uid = 0;
    msgHead.cmd = cmd;
    msgHead.msgLen = buffer.size();

    if (0 == buffer.size()) {
        m_socket->writeDatagram((char *)&msgHead, sizeof(PCP_UDP_MSG_HEAD), QHostAddress(m_udpRemoteIp.value().toString()), m_udpRemotePort.value().toInt());
    } else {
        QByteArray sendBuf;

        sendBuf.append((char *)&msgHead, sizeof(PCP_UDP_MSG_HEAD));
        sendBuf.append((char *)buffer.data(), buffer.size());
        m_socket->writeDatagram(buffer.data(), buffer.size(), QHostAddress(m_udpRemoteIp.value().toString()), m_udpRemotePort.value().toInt());
    }
}

void PCPUdpCtrl::setKickEnabled(bool enabled)
{
    if (enabled) {
        QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_KickEnable), Q_ARG(const QByteArray &, QByteArray()));
    } else {
        QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_KickDisable), Q_ARG(const QByteArray &, QByteArray()));
    }
}

void PCPUdpCtrl::setKickAbnormalEnabled(bool enabled)
{
    if (enabled) {
        QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_AbnormalKickEnable), Q_ARG(const QByteArray &, QByteArray()));
    } else {
        QMetaObject::invokeMethod(this, "sendDataAsync", Q_ARG(const uint16_t, PCPUdpCtrl::UdpCmd_AbnormalKickDisable), Q_ARG(const QByteArray &, QByteArray()));
    }
}

void PCPUdpCtrl::getNGCount(int &ngCount)
{
    ngCount = m_kickCount;
    m_kickCount = 0;
}

void PCPUdpCtrl::onReadyRead()
{
    PCP_UDP_MSG_HEAD *pMsgHead;
    char recvBuf[64];
    qint64 recvLen;
    static bool connected = false;

    while (m_socket->hasPendingDatagrams()) {
        memset(recvBuf, 0, sizeof(recvBuf));
        recvLen = m_socket->readDatagram(recvBuf, sizeof(recvBuf));
        if (recvLen < (qint64)sizeof(PCP_UDP_MSG_HEAD)) {
            return;
        }
        if (!connected) {
            //某些局点，IO板通信启动慢，导致启动阶段的参数未成功下发。增加首次接收IO消息，进行启动阶段参数的重发
            connected = true;
            setKickAbnormalEnabled(m_kickAbnormalEnable.value().toBool());
        }
        pMsgHead = (PCP_UDP_MSG_HEAD *)recvBuf;
        m_heartbeatLastTime = QDateTime::currentMSecsSinceEpoch();

        switch (pMsgHead->cmd) {
            case PCPUdpCtrl::UdpCmd_Reboot:
                if (m_enabled.value().toBool()) {
                    sendDataAsync(PCPUdpCtrl::UdpCmd_RunStart, QByteArray());
                }
                break;

            case PCPUdpCtrl::UdpCmd_InquireVersionInfo: {
                    PcpUdpVersionBody *pVersion = (PcpUdpVersionBody *)(recvBuf + sizeof(PCP_UDP_MSG_HEAD));
                    m_pcpVersionInfoList.clear();
                    m_pcpVersionInfoList.append(pVersion->moduleName);
                    m_pcpVersionInfoList.append(pVersion->moduleVersion);
                    m_pcpVersionInfoList.append(pVersion->publishDate);
                }
                break;

            case UdpCmd_Kick_Statistic: {
                    uint32_t *pValue = (uint32_t *)(recvBuf + sizeof(PCP_UDP_MSG_HEAD));
                    if (*pValue != 0) {
                        m_kickCount += *pValue;
                    }
                }
                break;

            case UdpCmd_Box_Shape_Sigal:
                emit boxShapeSignal();
                break;

            case PCPUdpCtrl::UdpCmd_HeardBeat:
            case PCPUdpCtrl::UdpCmd_KickStateNg:
            case PCPUdpCtrl::UdpCmd_BoxFull:
                emit sendDataToRealAlarm(*pMsgHead);
                break;

            default:
                break;
        }
    }
}
