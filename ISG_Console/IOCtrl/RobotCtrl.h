#ifndef ROBOTCTRL_H
#define ROBOTCTRL_H

#include "BaseLib/EngineObject.h"
#include <QThread>

class QTcpServer;
class QTcpSocket;

class RobotCtrl : public EngineObject
{
    Q_OBJECT
public:
    RobotCtrl(int id, EngineProject *project);
    ~RobotCtrl();
    static RobotCtrl *getInstance();
    virtual bool init();

private:
    static RobotCtrl *s_pInstance;

private slots:
    void onNewConnection();
    void onDisconnected();
    bool listenAsync();
    bool sendMsgAsync(float coordinateX, float coordinateY);

public:
    bool sendMsg(float coordinateX, float coordinateY);

private:
    Attribute m_tcpPort; //Tcp 端口号
    QTcpServer *m_server;
    QTcpSocket *m_socket;
    QThread m_thread; //Robot线程
};

#endif // ROBOTCTRL_H
