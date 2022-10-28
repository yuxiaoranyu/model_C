#include "PCPSerialCtrl.h"
#include <QDebug>

PCPSerialCtrl *PCPSerialCtrl::s_pInstance = NULL;

PCPSerialCtrl::PCPSerialCtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_kickEnable("connected", true, this),
    m_serialPortName("serial", "", this),
    m_attributeAdress("attribute", "", this),
    m_attributeValue("value", "false", this),
    m_snapAttributeAdress("snap_attribute", "", this),
    m_snapAttributeValue("snap_attribute_value", "false", this),
    m_serialPort(new QSerialPort(this))
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setParity(QSerialPort::NoParity);

    moveToThread(&m_thread);
    m_thread.setObjectName("PCP");
    m_thread.start();

    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(PCP_ReceiveBytes()));
}

PCPSerialCtrl::~PCPSerialCtrl()
{
    IodisConnect();

    m_thread.quit();
    m_thread.wait();

    s_pInstance = NULL;
}

PCPSerialCtrl *PCPSerialCtrl::getInstance()
{
    return s_pInstance;
}

bool PCPSerialCtrl::init()
{
    //init阶段，m_enabled标志尚未设置为true。
    //工程启停，不释放串口资源;
    //剔除功能使能，打开串口；剔除功能关闭，不打开串口
    if (m_kickEnable.value().toBool())
    {
        IoConnect();
    }

    //Attrbute配置恢复完成后，注册处理函数，防止配置恢复期间，处理函数依赖的参数未恢复完成。
    //剔除功能使能，打开串口；剔除功能关闭，不打开串口
    m_kickEnable.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool) {
        if (newValue.toBool() != oldValue.toBool())
        {
            if (newValue.toBool())
            {
                IoConnect();
            }
            else
            {
                IodisConnect();
            }
        }
        return true;
    });

    return true;
}

void PCPSerialCtrl::IoConnect()
{
    QMetaObject::invokeMethod(this, "IoConnectAsync");
}

void PCPSerialCtrl::IoConnectAsync()
{
    if (m_serialPort->isOpen())
    {
        m_serialPort->close();
    }

    m_serialPort->setPortName(m_serialPortName.value().toString());
    m_serialPort->open(QIODevice::ReadWrite);

    if (!m_serialPort->isOpen())
    {
        qInfo() << "open serial port " << m_serialPortName.value().toString() << " is failed!";
    }
    else
    {
        m_serialPort->clear();
    }
}

void PCPSerialCtrl::IodisConnect()
{
    QMetaObject::invokeMethod(this, "IodisConnectAsync");
}

void PCPSerialCtrl::IodisConnectAsync()
{
    if (m_serialPort->isOpen())
    {
        m_serialPort->close();
    }
}

void PCPSerialCtrl::PCP_KickSendData()
{
    if (!m_kickEnable.value().toBool())
    {
        return;
    }

    QString comName = m_attributeAdress.value().toString();
    QString valString = m_attributeValue.value().toString();

    QMetaObject::invokeMethod(this, "PCP_SendDataAsync", Q_ARG(QString, comName), Q_ARG(QString, valString));
}

void PCPSerialCtrl::TriggerCameraSnap()
{
    QString comName = m_snapAttributeAdress.value().toString();
    QString valString = m_snapAttributeValue.value().toString();

    QMetaObject::invokeMethod(this, "PCP_SendDataAsync", Q_ARG(QString, comName), Q_ARG(QString, valString));
}

void PCPSerialCtrl::PCP_SendDataAsync(const QString &comName, const QString &valString)
{
    char CommandBuff[MAX_PCP_MSG_BUFFER_SIZE];

    memset(CommandBuff, 0, sizeof(CommandBuff));

    if ((sizeof(PCP_MSG_HEAD) + comName.length() + 1 + valString.length() + 1) > MAX_PCP_MSG_BUFFER_SIZE)
    {
        qInfo() << "PCP_SendDataAsync attribute and value is too long!";
        return;
    }

    PCP_MSG_HEAD *pCommand = (PCP_MSG_HEAD *)&CommandBuff[0];
    pCommand->MsgCode = COMAND_SET_BATCH_ATTRIBUTE_VAL;
    pCommand->DataSegNum = 1;
    pCommand->iVal = 1;
    pCommand->MsgLen = sizeof(PCP_MSG_HEAD);

    QByteArray comNameByte = comName.toLocal8Bit();
    char *szAttrNames = comNameByte.data();
    strcpy((char *)CommandBuff + pCommand->MsgLen, szAttrNames);
    pCommand->MsgLen += (unsigned int)strlen(szAttrNames) + 1;

    QByteArray AttrValsByte = valString.toLocal8Bit();
    char *AttrVals = AttrValsByte.data();
    strcpy((char *)CommandBuff + pCommand->MsgLen, AttrVals);
    pCommand->MsgLen += (unsigned int)strlen(AttrVals) + 1;

    IoSendData((char *)pCommand, pCommand->MsgLen);
}

bool PCPSerialCtrl::IoSendData(const char *buf, const int len)
{
    char sendBuf[MAX_PCP_MSG_BUFFER_SIZE + sizeof(UART_FRAME_HEAD)];
    uint8_t sum = 0;

    if (!m_serialPort->isOpen())
    {
        m_serialPort->open(QIODevice::ReadWrite);
        if (m_serialPort->isOpen())
        {
            m_serialPort->clear();
        }
    }

    if (m_serialPort->isOpen())
    {
        memset(sendBuf, 0, sizeof(sendBuf));

        UART_FRAME_HEAD *pFrame = (UART_FRAME_HEAD *)sendBuf;

        pFrame->preamble = UART_PREAMBLE_CODE;
        pFrame->len = len;
        memcpy(pFrame->Data, buf, len);

        for (int i = 0; i < len; i++)
            sum += buf[i];
        pFrame->CRC = sum;

        qint64 writeLen = m_serialPort->write(sendBuf, (unsigned short)(len + sizeof(UART_FRAME_HEAD) - 2));
        if (writeLen == -1)
        {
            qInfo() << "serial port write failed, writed len is  -1!";
            m_serialPort->close();
            return false;
        }

        return true;
    }
    else
    {
        qInfo() << "open serial port " << m_serialPortName.value().toString() << " is failed!";

        return false;
    }
}

void PCPSerialCtrl::PCP_ReceiveBytes()
{
    qint64 bytes;
    UART_FRAME_HEAD ackFramehead;
    PCP_MSG_HEAD ackPcpMsgHead;

    while (true)
    {
        bytes = m_serialPort->bytesAvailable();
        if (bytes >= qint64(sizeof(UART_FRAME_HEAD) - 2 + sizeof(PCP_MSG_HEAD)))
        {
            m_serialPort->read((char *)&ackFramehead, sizeof(UART_FRAME_HEAD) - 2);
            m_serialPort->read((char *)&ackPcpMsgHead, sizeof(PCP_MSG_HEAD));
            bytes -= (sizeof(UART_FRAME_HEAD) - 2 + sizeof(PCP_MSG_HEAD));
            if (ackPcpMsgHead.MsgLen != sizeof(PCP_MSG_HEAD))
            {
                qInfo() << "serial port packet parse is error!";
                m_serialPort->close();
                return;
            }

            onMessagePCP_Ack(ackPcpMsgHead, QByteArray());
        }
        else
        {
            break;
        }
    }
}

void PCPSerialCtrl::onMessagePCP_Ack(const PCP_MSG_HEAD &msgHeader, const QByteArray &body)
{
    (void)body;

    qDebug() << "receive PCP Ack packet, return code is" << msgHeader.ReturnCode;
}
