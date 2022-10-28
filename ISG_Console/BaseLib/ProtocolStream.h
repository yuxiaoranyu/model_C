#ifndef PROTOCOLSTREAM_H
#define PROTOCOLSTREAM_H

#include <QObject>
#include "Protocol.h"

class QIODevice;

//接收Socket数据流，解析数据流，对数据流成包；发送报文数据接收信号，供TCP模块解析报文
//提供报文发送接口，进行数据流发送
class ProtocolStream : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolStream(QIODevice *device); // 以 device 作为父对象

signals:
    void messageCTRL(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body);   //系统接收控制消息，发送信号

public:
    QIODevice *device() const { return m_device; }
    bool sendMessage(const QByteArray &msg); //发送消息

private slots:
    void onReadyRead(); //响应socket的readyRead信号

private:
    QIODevice *m_device;  //QIODevice是socket的基类
    POLICY_INFO_MSG_HEADER m_wcpHead; //自定义的通信头
};

#endif // PROTOCOLSTREAM_H
