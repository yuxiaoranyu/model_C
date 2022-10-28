#include "IOCtrl/SiemensS7PLC.h"
#include <QDebug>
#include <QString>
#include <QDateTime>
#include <QJsonValue>

SiemensS7PLC *SiemensS7PLC::s_pInstance = NULL;

SiemensS7PLC::SiemensS7PLC(int id, EngineProject *project) :
    EngineObject(id, project),
    m_bConnected(false),
    m_lastConnectTime(QDateTime::currentDateTime()),
    m_ioIpAddress("ip", "127.0.0.1", this),
    m_ioRack("rack", 0, this),
    m_ioSlot("slot", 1, this),
    m_ioDataWidth("data_width", 2, this),
    m_ioDbNumber("db_number", 1, this),
    m_ioStartIndex("start_index", 0, this),
    m_ioWriteValue("write_value", 0, this),
    m_kickEnable("connected", true, this)
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    m_s7Client.SetConnectionType(CONNTYPE_BASIC);

    moveToThread(&m_thread);
    m_thread.setObjectName("PLC");
    m_thread.start();
}

SiemensS7PLC::~SiemensS7PLC()
{
    ioDisConnect();

    m_thread.quit();
    m_thread.wait();

    s_pInstance = NULL;
}

SiemensS7PLC *SiemensS7PLC::getInstance()
{
    return s_pInstance;
}

bool SiemensS7PLC::init()
{
    if (m_kickEnable.value().toBool())
    {
        ioConnect();
    }

    m_kickEnable.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool) {
        if (newValue.toBool() != oldValue.toBool())
        {
            if (newValue.toBool())
            {
                ioConnect();
            }
            else
            {
                ioDisConnect();
            }
        }
        return true;
    });

    return true;
}

void SiemensS7PLC::ioConnect()
{
    QMetaObject::invokeMethod(this, "ioConnectAsync");
}

void SiemensS7PLC::ioDisConnect()
{
    QMetaObject::invokeMethod(this, "ioDisConnectAsync");
}

void SiemensS7PLC::sendDataToPLC()
{
    if (!m_kickEnable.value().toBool())
    {
        return;
    }

    QMetaObject::invokeMethod(this, "sendDataToPLCAsync");
}

void SiemensS7PLC::ioConnectAsync()
{
    QString strIp = m_ioIpAddress.value().toString();
    if (auto error = m_s7Client.ConnectTo(strIp.toStdString().c_str(), m_ioRack.value().toInt(), m_ioSlot.value().toInt()))
    {
        qInfo().noquote() << tr("Connect PLC error! code: %1").arg(error);
    }
    else
    {
        m_bConnected = true;
        qDebug() << "Connect PLC success!";
    }
}

void SiemensS7PLC::ioDisConnectAsync()
{
    m_bConnected = false;

    if (auto error = m_s7Client.Disconnect())
    {
        qInfo().noquote() << tr("Disconnect PLC error! code: %1").arg(error);
    }
    else
    {
        qDebug() << "Disconnect PLC success!";
    }
}

void SiemensS7PLC::sendDataToPLCAsync()
{
    if (!m_bConnected)
    {
        ioReConnect();
    }

    if (!m_bConnected)
    {
        return;
    }

    int db = m_ioDbNumber.value().toInt();
    int index = m_ioStartIndex.value().toInt();
    int size = m_ioDataWidth.value().toInt();
    int val = m_ioWriteValue.value().toInt();

    if (size > (int)sizeof(int))
    {
        qInfo() << "PLC: data_width is wrong!";
        return;
    }

    if (0 != m_s7Client.DBWrite(db, index, size, &val))
    {
        qInfo() << "PLC: SendData error!";
        ioReConnect();
    }
    else
    {
        qDebug() << "PLC: SendData success";
    }
}

void SiemensS7PLC::ioReConnect()
{
    qDebug() << "Try reconnecting PLC";

    QDateTime now = QDateTime::currentDateTime();
    if (now.toSecsSinceEpoch() - m_lastConnectTime.toSecsSinceEpoch() >= 5)
    {
        ioDisConnectAsync();
        ioConnectAsync();
        m_lastConnectTime = now;
    }
}
