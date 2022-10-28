#include "PLCUdpCtrl.h"
#include <QUdpSocket>
#include <QByteArray>
#include <QDebug>
#include <QtEndian>

PLCUdpCtrl *PLCUdpCtrl::s_pInstance = NULL;

PLCUdpCtrl::PLCUdpCtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_socket(new QUdpSocket(this)),
    m_udpLocalIp("local_ip", "0.0.0.0", this),
    m_udpLocalPort("local_port", 1234, this),
    m_udpRemoteIp("remote_ip", "0.0.0.0", this),
    m_udpRemotePort("remote_port", 1234, this),
    m_kickEnable("connected", true, this)
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

PLCUdpCtrl::~PLCUdpCtrl()
{
}

bool PLCUdpCtrl::init()
{
    m_socket->bind(QHostAddress(m_udpLocalIp.value().toString()), m_udpLocalPort.value().toInt());

    moveToThread(&m_thread);
    m_thread.setObjectName("PlcThread");
    m_thread.start();
    return true;
}

void PLCUdpCtrl::cleanup()
{
    m_thread.quit();
    m_thread.wait();
}

PLCUdpCtrl *PLCUdpCtrl::getInstance()
{
    return s_pInstance;
}

void PLCUdpCtrl::writeData(int x, int y)
{
    QMetaObject::invokeMethod(this, "writeDataAsync", Q_ARG(int, x), Q_ARG(int, y));
}

void PLCUdpCtrl::writeDataAsync(int x, int y)
{
    PLC_UDP_MSG msg;

    msg.magic   = qToBigEndian((uint32_t)PLC_UDP_MAGIC);
    msg.cmd     = qToBigEndian((uint32_t)PLC_UDP_CMD_SEND);
    msg.msgLen  = qToBigEndian((uint32_t)(sizeof(msg.label_x) + sizeof(msg.label_x)));
    msg.label_x = qToBigEndian((uint32_t)x);
    msg.label_y = qToBigEndian((uint32_t)y);

    qInfo() << "PLCUdpCtrl: x =" << x << ", y =" << y;
    m_socket->writeDatagram((char *)&msg, sizeof(msg), QHostAddress(m_udpRemoteIp.value().toString()), m_udpRemotePort.value().toInt());
}

void PLCUdpCtrl::onReadyRead()
{
    char recvBuf[256];

    while (m_socket->hasPendingDatagrams()) {
        memset(recvBuf, 0, sizeof(recvBuf));
        qint64 read_len = m_socket->readDatagram(reinterpret_cast<char *>(recvBuf), sizeof(recvBuf));
        if (-1 == read_len) {
           return;
        }
        qDebug() << "UDP get data: " << recvBuf;
    }
}
