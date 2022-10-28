#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "IOCtrl/PCPUdpCtrl.h"
#include "BaseLib/EngineObject.h"
#include "BaseLib/Protocol.h"

#include <QThread>
#include <QImage>
#include <QJsonDocument>

class QTcpSocket;
class QTimer;

class ProtocolStream;

enum PointDetectStatus {
    ON_DETECT = 0,  //正在点检
    FINISH_DETECT,  //终止点检
    STOP_DETECT,    //暂停点检
};

//与策略中心的TCP连接管理线程
class TcpClient : public EngineObject
{
    Q_OBJECT

public:
    explicit TcpClient(EngineProject *project);
    void start();
    void respondToPolicyStore(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj);  // 应答策略中心的消息
    void sendToPolicyStore(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj);     // 向策略中心发送消息

signals:
    void sendDataToRealAlarm(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body);       // 系统接收控制消息，发送信号

protected:
    void cleanup();

private slots:
    void onConnected();
    void onDisconnected();
    void onTimeout();
    void onStatisticReportTimeout();        // 统计信息上报
    void onMessageCTRL(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body);
    void sendToPolicyStoreAsync(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj);

private:
    void registerToPolicyStore();           // 向策略中心注册
    void setParam(const POLICY_INFO_MSG_HEADER &head, QJsonObject &jsonObj); // 参数配置处理函数

private:
    QThread        m_thread;                // 独立线程
    QTcpSocket     *m_socket;               // TCP Socket
    ProtocolStream *m_stream;               // socket数据流接收、发送封装
    QTimer         *m_timer;                // 定时器
    QTimer         *m_statisticReportTimer; // 统计信息上报定时器
    int32_t        m_detectStatus;          // 当前点检状态 （点检模式有效）
    uint64_t       m_registerMsgTransId;    // 注册请求消息事务ID
    bool           m_bCleaned;              // 系统退出的clean标志
};

#endif // TCPCLIENT_H
