#include "ProtocolStream.h"
#include "EngineProject.h"
#include <QByteArray>
#include <QDebug>
#include <QIODevice>

ProtocolStream::ProtocolStream(QIODevice *device) :
    QObject(device),
    m_device(device)
{
    Q_ASSERT(device);
    connect(device, SIGNAL(readyRead()), SLOT(onReadyRead()));
}

bool ProtocolStream::sendMessage(const QByteArray &msg)
{
    auto data = msg.data();
    auto len = msg.size();
    qint64 bytes;

    while (len > 0)
    {
        bytes = m_device->write(data, len);
        if (bytes < 0)
        {
            qInfo().noquote() << tr("ProtocolStream: failed to send message");
            return false;
        }
        data += bytes;
        len -= bytes;
    }
    return true;
}

void ProtocolStream::onReadyRead()
{
    qint64 bytesAvailable;
    
    while (true)
    {
        bytesAvailable = m_device->bytesAvailable();
        if (bytesAvailable >= qint64(sizeof m_wcpHead)) {
            m_device->startTransaction();
            m_device->read(reinterpret_cast<char *>(&m_wcpHead), sizeof m_wcpHead);
            bytesAvailable -= sizeof m_wcpHead;
            if (bytesAvailable >= m_wcpHead.len) {
                if (m_wcpHead.len > 0) {
                    //信号的响应函数与信号的发送需同一个线程。
                    //保证信号处理完成后，才跳出emit函数，防止内存已释后，异步才响应
                    emit messageCTRL(m_wcpHead, m_device->read(m_wcpHead.len));
                } else {
                    emit messageCTRL(m_wcpHead, QByteArray());
                }
                m_device->commitTransaction();
                continue;
            }
            m_device->rollbackTransaction();
        }
        break;
    }
}
