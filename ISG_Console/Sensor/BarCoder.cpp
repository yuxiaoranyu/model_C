#include "BarCoder.h"
#include <QDebug>

BarCoder *BarCoder::s_pInstance = NULL;

BarCoder::BarCoder(int id, EngineProject *project) :
    EngineObject(id, project),
    m_timer(new QTimer(this)),
    m_serialPort(new QSerialPort(this)),
    m_serialPortName("serial", "", this),
    m_connected(false)
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    m_timer->setInterval(2000);
    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));

    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setParity(QSerialPort::NoParity);

    moveToThread(&m_thread);
    m_thread.setObjectName("BarCoder");
    m_thread.start();

    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(BarCoderReceiveBytes()));
}

BarCoder::~BarCoder()
{
    s_pInstance = NULL;
}

BarCoder *BarCoder::getInstance()
{
    return s_pInstance;
}

bool BarCoder::init()
{
    QMetaObject::invokeMethod(this, "BarCoderConnectAsync");
    m_timer->start();

    return true;
}

void BarCoder::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(this, "BarCoderDisConnectAsync");
    m_thread.quit();
    m_thread.wait();
}

void BarCoder::BarCoderConnectAsync()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->clear();
        m_serialPort->close();
    }

    m_serialPort->setPortName(m_serialPortName.value().toString());
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        m_connected = false;
        qInfo() << "open serial port " << m_serialPortName.value().toString() << " failed!";
    } else {
        m_connected = true;
        m_serialPort->clear();
    }
}

void BarCoder::BarCoderDisConnectAsync()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->clear();
        m_serialPort->close();
    }
}

void BarCoder::BarCoderReceiveBytes()
{
    QByteArray codeData;

    qint64 startIndex;
    qint64 endIndex;

    while (true) {
        qint64 bytes = m_serialPort->bytesAvailable();
        if (bytes > 0) {
            m_readData.append(m_serialPort->readAll().data());

            startIndex = m_readData.indexOf('\x02');   //条码开始标志
            if (startIndex == -1) {
                m_readData.clear();
            } else if (startIndex > 0) {
                m_readData = m_readData.mid(startIndex);
            }

            endIndex = m_readData.indexOf('\x03');   //条码结束标志
            if (endIndex < 0) {
                continue;
            }
            //get the code
            codeData = m_readData.mid(startIndex+1, endIndex-startIndex-1);
            emit receiveCoder(codeData);
            //处理过的数据丢弃
            m_readData = m_readData.mid(endIndex + 1);
        } else {
            break;
        }
    }
}

void BarCoder::onTimeout()
{
    if (!m_serialPort->isOpen())
    {
        m_serialPort->open(QIODevice::ReadWrite);
        if (m_serialPort->isOpen())
        {
            m_connected = true;
            m_serialPort->clear();
        } else {
            m_connected = false;
        }
    }
}
