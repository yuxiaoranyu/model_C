#include "RobotCtrl.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>

RobotCtrl *RobotCtrl::s_pInstance = NULL;

RobotCtrl::RobotCtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_tcpPort("tcp_port", 9000, this),
    m_server(new QTcpServer(this)),
    m_socket(nullptr)
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    connect(m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

    moveToThread(&m_thread);
    m_thread.setObjectName("Robot");
    m_thread.start();
}

RobotCtrl::~RobotCtrl()
{
    m_thread.quit();
    m_thread.wait();
}

RobotCtrl *RobotCtrl::getInstance()
{
    return s_pInstance;
}

bool RobotCtrl::init()
{
    bool ok = false;
    QMetaObject::invokeMethod(this, "listenAsync", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok));
    return ok;
}

bool RobotCtrl::listenAsync()
{
    if (m_server->listen(QHostAddress::Any, m_tcpPort.value().toInt())) {
        qDebug() << "Robot TcpServer: listening...";
        return true;
    } else {
        qInfo() << "Robot TcpServer: listen failed";
        return false;
    }
}

void RobotCtrl::onNewConnection()
{
    m_socket = m_server->nextPendingConnection();
    connect(m_socket, SIGNAL(disconnected()), SLOT(onDisconnected()));
    qInfo() << "Robot TcpServer: new client connect!";
}

void RobotCtrl::onDisconnected()
{
    sender()->deleteLater();
    m_socket = nullptr;
    qInfo() << "Robot TcpServer: client disconnected!";
}

bool RobotCtrl::sendMsg(float coordinateX, float coordinateY)
{
    bool result = false;
    QMetaObject::invokeMethod(this, "sendMsgAsync",
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(float, coordinateX),
                              Q_ARG(float, coordinateY));
    return result;
}

bool RobotCtrl::sendMsgAsync(float coordinateX, float coordinateY)
{
    if (m_socket && m_socket->isOpen()) {
        QString msg = tr("Image\r\n[X:%1;Y:%2;A:0]\r\nDone").arg(coordinateX)
                                                            .arg(coordinateY);
        QByteArray data = msg.toUtf8();
        if (m_socket->write(data) == -1) {
            qInfo() << "Robot TcpServer: write error";
            return false;
        }

        return true;
    } else {
        qDebug() << "No robot client connection!";
        return false;
    }
}
