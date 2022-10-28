#include "IopAgent/TcpServer.h"
#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaType>
#include "BaseLib/EngineProject.h"
#include "BaseLib/ProtocolStream.h"

TcpServer::TcpServer(EngineProject *project) :
    EngineObject(project),
    m_server(new QTcpServer(this))
{
    connect(m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    moveToThread(&m_thread);
    m_thread.setObjectName("TcpServer");
    m_thread.start();
}

bool TcpServer::listen()
{
    bool ok = false;
    QMetaObject::invokeMethod(this, "listenAsync", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok));
    return ok;
}

bool TcpServer::listenAsync()
{
    if (m_server->listen(QHostAddress::Any, project()->tcpPort()))
    {
        qDebug() << "IOP TcpServer: listening...";
        return true;
    }
    else
    {
        qInfo() << "IOP TcpServer: listen failed";
        return false;
    }
}

void TcpServer::onNewConnection()
{
    auto sock = m_server->nextPendingConnection();

    if (m_streamMap.size() < MAX_TCP_CLIENT)
    {
        auto stream = new ProtocolStream(sock);
        m_streamMap.insert(sock, stream);

        connect(sock, SIGNAL(disconnected()), SLOT(onDisconnected()));
        connect(stream, SIGNAL(messageCTRL(CTRL_MSG_HEADER, QByteArray)),
                SLOT(onMessageCTRL(CTRL_MSG_HEADER, QByteArray)));

        qDebug() << "IOP TcpServer: New connection accepted!! client num:" << m_streamMap.size();
    }
    else
    {
        qInfo() << "IOP TcpServer: MAX connect, error!";
        sock->disconnectFromHost();
        sock->deleteLater();
    }
}

void TcpServer::onDisconnected()
{
    m_streamMap.remove(sender());
    sender()->deleteLater();

    qDebug() << "IOP TcpServer: Client disconnected!! client num:" << m_streamMap.size();
}

void TcpServer::reportImageResult(int camerId, uint64_t imgId, const QImage &image, int result, const QJsonDocument &desc)
{
    QMetaObject::invokeMethod(this, "reportImageResultAsync",
                              Q_ARG(int, camerId),
                              Q_ARG(uint64_t, imgId),
                              Q_ARG(QImage, image),
                              Q_ARG(int, result),
                              Q_ARG(QJsonDocument, desc));
}

void TcpServer::cleanup()
{
    QMetaObject::invokeMethod(m_server, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void TcpServer::reportImageResultAsync(int camerId, uint64_t imgId, const QImage &image, int result, const QJsonDocument &desc)
{
    if (m_streamMap.isEmpty())
    {
        return;
    }

    QByteArray msg(sizeof(IMGE_MSG_HEADER), Qt::Uninitialized);
    {
        QBuffer buffer(&msg);
        buffer.open(QIODevice::Append);
        image.save(&buffer, "jpg");
    }

    // 图像类型检查
    uint16_t imageType;
    switch (image.format())
    {
    case QImage::Format_Grayscale8:
        imageType = PIXEL_FORMAT_MONO8;
        break;
    case QImage::Format_BGR888:
        imageType = PIXEL_FORMAT_RGB24;
        break;
    case QImage::Format_RGB32:
        imageType = PIXEL_FORMAT_RGB32;
        break;
    default:
        // 其他格式统一转换为BGR888
        imageType = PIXEL_FORMAT_RGB24;
        break;
    }

    IMGE_MSG_HEADER *header = reinterpret_cast<IMGE_MSG_HEADER *>(msg.data());
    header->flag = IMGE_MSG_FLAG;
    header->seqNo = 0;
    header->currTime = QDateTime::currentMSecsSinceEpoch();
    header->moduleNo = ISG_MODULE;
    header->cmdCode = IMGE_REPORT;
    header->version = ISG_VERSION1;
    header->imageId = imgId;
    header->result = result;
    header->imageHeight = image.height();
    header->imageWidth = image.width();
    header->imageType = imageType;
    header->msgLen = msg.size() - sizeof(IMGE_MSG_HEADER);

    QByteArray jsonQByteArray;
    if (!desc.isNull())
    {
        jsonQByteArray = desc.toJson(QJsonDocument::Compact);
    }

    IMGE_MSG_HEADER resHeader;
    resHeader.flag = IMGE_MSG_FLAG;
    resHeader.seqNo = 0;
    resHeader.currTime = QDateTime::currentMSecsSinceEpoch();
    resHeader.moduleNo = ISG_MODULE;
    resHeader.cmdCode = IMGE_RESULT;
    resHeader.version = ISG_VERSION1;
    resHeader.imageId = imgId;
    resHeader.result = result;
    resHeader.imageHeight = image.height();
    resHeader.imageWidth = image.width();
    resHeader.imageType = imageType;
    resHeader.msgLen = jsonQByteArray.size();
    msg.append((char *)&resHeader, sizeof(resHeader));
    msg.append(jsonQByteArray);

    for (auto stream : m_streamMap)
    {
        stream->sendMessage(msg);
    }
}

/*
控制命令的json格式：
[
    [组件名,字段,值],
    [组件名,字段,值],
    ...
]
例如：
[
    ["camera-1","gain",1],
    ["global","enable",false],
    ...
]

应答控制命令的json格式：
成功：
{
    "code":200
}
失败：
{
    "code":300,
    "which":[1,2,3] //设置失败的命令编号
}

获取参数命令的json格式：
[
    [组件名,字段],
    [组件名,字段],
    ...
]

应答获取参数命令的json格式：
[
    [组件名,字段,值],
    [组件名,字段,值],
    ...
]
*/
void TcpServer::onMessageCTRL(const CTRL_MSG_HEADER &header, const QByteArray &body)
{
    ProtocolStream *stream = static_cast<ProtocolStream *>(sender());

    if (header.moduleNo == IOP_MODULE && header.cmdCode == PARAM_SET)
    {
        qDebug().noquote() << tr("IOP TcpServer: Set json: %1").arg(body.data());

        QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonArray ctrlArray = doc.array();

        ObjectPtr ptr;
        QJsonArray failArr;

        for (int i = 0; i < ctrlArray.size(); i++)
        {
            QJsonArray arr = ctrlArray[i].toArray();
            if (arr.size() == 3)
            {
                QString componentName = arr[0].toString();
                QString paramName = arr[1].toString();
                QJsonValue value = arr[2];
                if (componentName == project()->name())
                {
                    if (project()->setAttr(paramName, value))
                    {
                        continue;
                    }
                }
                else
                {
                    ptr = project()->findObject(componentName);
                    if (ptr && ptr->setAttr(paramName, value))
                    {
                        continue;
                    }
                }
            }
            failArr.append(i);
        }

        QJsonObject ctrlResult;
        if (failArr.isEmpty())
        {
            ctrlResult.insert("code", CTRL_SUCCESS);
        }
        else
        {
            ctrlResult.insert("code", CTRL_ERROR);
            ctrlResult.insert("which", failArr);
        }

        doc = QJsonDocument(ctrlResult);
        QByteArray array = doc.toJson(QJsonDocument::Compact);

        qDebug().noquote() << tr("IOP TcpServer: Respond tcp set json: %1").arg(array.data());

        CTRL_MSG_HEADER ack;
        ack.flag = CTRL_MSG_FLAG;
        ack.seqNo = header.seqNo;
        ack.currTime = QDateTime::currentMSecsSinceEpoch();
        ack.moduleNo = ISG_MODULE;
        ack.cmdCode = PARAM_ACK;
        ack.version = ISG_VERSION1;
        ack.msgLen = array.size();

        QByteArray msg;
        msg.append((char *)&ack, sizeof(ack));
        msg.append(array);

        stream->sendMessage(msg);
    }
    else if (header.moduleNo == IOP_MODULE && header.cmdCode == PARAM_GET)
    {
        qDebug().noquote() << tr("IOP TcpServer: Get json: %1").arg(body.data());

        QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonArray getValueArray = doc.array();
        ObjectPtr ptr;

        for (int i = 0; i < getValueArray.size(); i++)
        {
            QJsonArray arr = getValueArray[i].toArray();
            if (arr.size() == 2)
            {
                QString componentName = arr[0].toString();
                QString paramName = arr[1].toString();

                if (componentName == project()->name())
                {
                    arr.append(project()->attr(paramName));
                    getValueArray[i] = arr;
                }
                else
                {
                    ptr = project()->findObject(componentName);
                    if (ptr)
                    {
                        arr.append(ptr->attr(paramName));
                        getValueArray[i] = arr;
                    }
                }
            }
        }

        doc = QJsonDocument(getValueArray);
        QByteArray array = doc.toJson(QJsonDocument::Compact);

        qDebug().noquote() << tr("IOP TcpServer: Respond tcp get json: %1").arg(array.data());

        CTRL_MSG_HEADER ack;
        ack.flag = CTRL_MSG_FLAG;
        ack.seqNo = header.seqNo;
        ack.currTime = QDateTime::currentMSecsSinceEpoch();
        ack.moduleNo = ISG_MODULE;
        ack.cmdCode = PARAM_ACK;
        ack.version = ISG_VERSION1;
        ack.msgLen = array.size();

        QByteArray msg;
        msg.append((char *)&ack, sizeof(ack));
        msg.append(array);
        stream->sendMessage(msg);
    }
    else
    {
        qInfo() << "IOP TcpServer: Msg header error:" << header.moduleNo << "|" << header.cmdCode;
    }
}
