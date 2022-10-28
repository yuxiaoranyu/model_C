#ifndef SIEMENSS7PLC_H
#define SIEMENSS7PLC_H

#include "BaseLib/EngineObject.h"
#include "ThirdParty/Snap7/s7_client.h"
#include <QDateTime>
#include <QThread>

class SiemensS7PLC : public EngineObject
{
    Q_OBJECT
public:
    SiemensS7PLC(int id, EngineProject *project);
    ~SiemensS7PLC();
    static SiemensS7PLC *getInstance();
    virtual bool init();

    void sendDataToPLC();       //向下位机发送数据

private:
    static SiemensS7PLC *s_pInstance;

private slots:
    void ioConnectAsync();      //连接下位机槽
    void ioDisConnectAsync();   //断开下位机槽
    void sendDataToPLCAsync();  //向下位机发送数据槽

private:
    void ioConnect();       //连接下位机
    void ioDisConnect();    //断开下位机
    void ioReConnect();     //重连下位机

private:
    bool m_bConnected;           //连接状态
    QDateTime m_lastConnectTime; //上一次重连时间

    Attribute m_ioIpAddress;  //设备IP地址
    Attribute m_ioRack;       //设备Rack
    Attribute m_ioSlot;       //设备Slot
    Attribute m_ioDataWidth;  //数据写入的宽度
    Attribute m_ioDbNumber;   //数据写入的数据块编号
    Attribute m_ioStartIndex; //数据写入的起始位置
    Attribute m_ioWriteValue; //数据写入的值
    Attribute m_kickEnable;   //是否使能剔除

    QThread m_thread; //PLC线程
    TSnap7Client m_s7Client;
};

#endif // SIEMENSS7PLC_H
