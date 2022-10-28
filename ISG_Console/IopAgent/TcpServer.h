#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QThread>
#include "BaseLib/EngineObject.h"
#include "BaseLib/Protocol.h"

#define MAX_TCP_CLIENT 5

class QTcpServer;
class ProtocolStream;

class TcpServer : public EngineObject
{
    Q_OBJECT

public:
    explicit TcpServer(EngineProject *project);

public:
    bool listen();
    void reportImageResult(int camerId, uint64_t imgId, const QImage &image, int result, const QJsonDocument &desc);

protected:
    void cleanup();

private slots:
    bool listenAsync();
    void onDisconnected();
    void onMessageCTRL(const CTRL_MSG_HEADER &header, const QByteArray &body);
    void onNewConnection();
    void reportImageResultAsync(int camerId, uint64_t imgId, const QImage &image, int result, const QJsonDocument &desc);

private:
    QHash<QObject *, ProtocolStream *> m_streamMap; // tcp sock -> stream
    QTcpServer *m_server;
    QThread m_thread;
};

#endif // TCPSERVER_H
