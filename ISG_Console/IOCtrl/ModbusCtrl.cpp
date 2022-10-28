#include "ModbusCtrl.h"
#include <QDebug>
#include <QByteArray>
#include <QTcpSocket>
#include <QModbusTcpServer>

#define REGISTER_MAX_SIZE  1024   //内部寄存器的大小

ModbusCtrl *ModbusCtrl::s_pInstance = NULL;

ModbusCtrl::ModbusCtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_localIp("ip", "127.0.0.1", this),
    m_localPort("port", 502, this),
    m_modbusDevice(new QModbusTcpServer(this))
{
    Q_ASSERT(!s_pInstance);
    s_pInstance = this;

    connect(m_modbusDevice, SIGNAL(modbusClientDisconnected(QTcpSocket *)), this, SLOT(onDisconnected(QTcpSocket *)));
}

ModbusCtrl::~ModbusCtrl()
{
    m_thread.quit();
    m_thread.wait();

    s_pInstance = NULL;
}

ModbusCtrl *ModbusCtrl::getInstance()
{
    return s_pInstance;
}

bool ModbusCtrl::init()
{
    QModbusDataUnitMap reg;
    reg.insert(QModbusDataUnit::Coils, { QModbusDataUnit::Coils, 0, REGISTER_MAX_SIZE });
    reg.insert(QModbusDataUnit::DiscreteInputs, { QModbusDataUnit::DiscreteInputs, 0, REGISTER_MAX_SIZE });
    reg.insert(QModbusDataUnit::InputRegisters, { QModbusDataUnit::InputRegisters, 0, REGISTER_MAX_SIZE });
    reg.insert(QModbusDataUnit::HoldingRegisters, { QModbusDataUnit::HoldingRegisters, 0, REGISTER_MAX_SIZE });

    m_modbusDevice->setMap(reg);

    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_localIp.value().toString());
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_localPort.value().toInt());
    m_modbusDevice->setServerAddress(1);

    if (!m_modbusDevice->connectDevice()) {
        qInfo().noquote() << tr("Modbus TcpServer: init failed, Error code: %1").arg(m_modbusDevice->errorString());
        return false;
    } else {
        qDebug() << "Modbus TcpServer: init seccuss";
    }

    moveToThread(&m_thread);
    m_thread.setObjectName("Modbus");
    m_thread.start();

    return true;
}

void ModbusCtrl::onDisconnected(const QTcpSocket *const tcpClient)
{
    Q_UNUSED(tcpClient)
    qInfo() << "Modbus TcpServer: client disconnected";
}

void ModbusCtrl::cleanup()
{
    if (m_modbusDevice) {
        m_modbusDevice->disconnect();
        delete m_modbusDevice;
        m_modbusDevice = nullptr;
    }
}

bool ModbusCtrl::setIOValue(const QString &IOAddr, int value)
{
    ADDR_TYPE AddrType = (ADDR_TYPE)(IOAddr.toInt() / 10000);
    int addr = (IOAddr.toInt() % 10000) - 1;

    if (addr >= REGISTER_MAX_SIZE) {
        qInfo() << "Modbus: Address exceeds max, setIOValue failed";
        return false;
    }

    if (AddrType == ADDR_OUTPUT_BIT) {
        m_modbusDevice->setData(QModbusDataUnit::Coils, addr, value);
    } else if (AddrType == ADDR_INPUT_BIT) {
        m_modbusDevice->setData(QModbusDataUnit::DiscreteInputs, addr, value);
    } else if (AddrType == ADDR_INPUT_REGISTER) {
        m_modbusDevice->setData(QModbusDataUnit::InputRegisters, addr, value);
    } else if (AddrType == ADDR_OUTPUT_REGISTER) {
        m_modbusDevice->setData(QModbusDataUnit::HoldingRegisters, addr, value);
    } else {
        qInfo() << "Modbus: Invalid address, setIOValue failed";
        return false;
    }

    qDebug() << "Modbus: setIOValue success";
    return true;
}

int ModbusCtrl::getIOValue(const QString &IOAddr)
{
    ADDR_TYPE AddrType = (ADDR_TYPE)(IOAddr.toInt() / 10000);
    int addr = (IOAddr.toInt() % 10000) - 1;

    if (addr >= REGISTER_MAX_SIZE) {
        qInfo() << "Modbus: Address exceeds max, getIOValue failed";
        return 0;
    }

    quint16 val;
    if (AddrType == ADDR_OUTPUT_BIT) {
        m_modbusDevice->data(QModbusDataUnit::Coils, addr, &val);
    } else if (AddrType == ADDR_INPUT_BIT) {
        m_modbusDevice->data(QModbusDataUnit::DiscreteInputs, addr, &val);
    } else if (AddrType == ADDR_INPUT_REGISTER) {
        m_modbusDevice->data(QModbusDataUnit::InputRegisters, addr, &val);
    } else if (AddrType == ADDR_OUTPUT_REGISTER) {
        m_modbusDevice->data(QModbusDataUnit::HoldingRegisters, addr, &val);
    } else {
        qInfo() << "Modbus: Invalid address, getIOValue failed";
        return 0;
    }

    qDebug() << "Modbus: getIOValue success";
    return val;
}
