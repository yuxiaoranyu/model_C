#ifndef MODBUSCTRL_H
#define MODBUSCTRL_H

#include "BaseLib/EngineObject.h"
#include <QThread>

class QModbusTcpServer;
class QTcpSocket;

class ModbusCtrl : public EngineObject
{
    Q_OBJECT

public:
    enum ADDR_TYPE{
        ADDR_OUTPUT_BIT = 0,
        ADDR_INPUT_BIT = 1,
        ADDR_INPUT_REGISTER = 3,
        ADDR_OUTPUT_REGISTER  = 4
    };

public:
    ModbusCtrl(int id, EngineProject *project);
    ~ModbusCtrl();
    static ModbusCtrl *getInstance();
    bool init() override;
    void cleanup() override;

public:
    bool setIOValue(const QString &IOAddr, int value);
    int getIOValue(const QString &IOAddr);

private:
    static ModbusCtrl *s_pInstance;

private slots:
    void onDisconnected(const QTcpSocket *const tcpClient);

private:
    Attribute m_localIp; //Modbus 地址
    Attribute m_localPort; //Modbus 端口号
    QModbusTcpServer *m_modbusDevice;
    QThread m_thread; //Modbus线程
};

#endif // MODBUSCTRL_H
