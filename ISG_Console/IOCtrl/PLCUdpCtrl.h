#ifndef PLCUDPCTRL_H
#define PLCUDPCTRL_H

#include "BaseLib/EngineObject.h"
#include <QThread>
#include <QObject>

//烟丝除杂产品，通过PLC传递杂物坐标，供PLC进行杂物剔除

#define PLC_UDP_MAGIC                       0x55AA55AA  //PCP_UDP消息魔术字
#define PLC_UDP_CMD_SEND                    1           //发送剔除命令

class QUdpSocket;

#pragma pack(1)

struct PLC_UDP_MSG //与PCP_UDP单板通信的帧头
{
    uint32_t magic;     //消息魔术字，固定为0x55AA55AA
    uint32_t cmd;       //消息命令字，表项下发定义为1，回应消息定义为100，可扩充
    uint32_t msgLen;    //消息体长度，不包括消息头
    uint32_t label_x;
    uint32_t label_y;
};

#pragma pack()

class PLCUdpCtrl  : public EngineObject
{
    Q_OBJECT

public:
    PLCUdpCtrl(int id, EngineProject *project);
    ~PLCUdpCtrl(void);
    static PLCUdpCtrl *getInstance(); //单实例
    bool init() override;
    void cleanup() override;
    void writeData(int x, int y);

private slots:
    void onReadyRead();
    void writeDataAsync(int x, int y);

private:
    static PLCUdpCtrl *s_pInstance;  //单实例

private:
    QThread m_thread; //plc udp 线程
    QUdpSocket *m_socket;

    Attribute  m_udpLocalIp;            // 本地地址
    Attribute  m_udpLocalPort;          // 本地端口号
    Attribute  m_udpRemoteIp;           // 目的地址
    Attribute  m_udpRemotePort;         // 目的端口号
    Attribute  m_kickEnable;            // 是否使能剔除
};

#endif // PLCUDPCTRL_H
