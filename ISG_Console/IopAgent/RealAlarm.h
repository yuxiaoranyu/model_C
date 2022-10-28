#ifndef REALALARM_H
#define REALALARM_H

#include <QThread>
#include <QTimer>
#include "IOCtrl/PCPUdpCtrl.h"
#include "BaseLib/EngineObject.h"
#include "BaseLib/Protocol.h"

#define STATE_ABNORMAL          0           // 状态异常
#define STATE_NORMAL            1           // 状态正常

//实时告警管理模块
class RealAlarm : public EngineObject
{
    Q_OBJECT

public:
    explicit RealAlarm(EngineProject *project);  //构造函数

public:
    bool init();  //初始化函数
    void cleanup();  //清除函数
    void triggerReport(); //烟丝除杂停机模式，检测出杂物后，设置系统停机；ISG更改自身状态，快速触发一次状态上报；

private slots:
    void onTimeout(); //超时处理
    void onTcpAlarm(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body);  //处理来自policy store的告警消息
    void onUdpIoAlarm(const PCP_UDP_MSG_HEAD &head);                              //处理来自UDP IO单板的告警
    void onTriggerReport();                                                       //状态上报Slot

private:
    QThread m_thread;
    QTimer  *m_timer;

    uint16_t m_udpIoLedStateCmd;     //UDP IO单板的亮灯状态: 绿灯/红灯
    uint16_t m_reportInterval;       //上报policy store的时间间隔,单位秒,policy store下发的参数
    int      m_reportControlCnt;     //上报控制，设备有异常或计数达到reportInterval，进行上报

    uint8_t  m_continueKickEnable;   //连续踢除使能开关,policy store下发的参数
    uint8_t  m_continueKickCount;    //连踢个数,policy store下发的参数
    bool     m_isContinueKickSet;    //是否已接收policy store下发的连踢参数

    bool     m_isUdpIoOnline;        //UDP IO单板是否在线
    int      m_boxStatus;            //小包、条包产品满箱告警
    int      m_kickAckStatus;        //小包、条包产品剔除确认光纤未感应告警

    void respondToPolicyStore(const POLICY_INFO_MSG_HEADER &msgHead, bool isSuccess);
    void heartbeatToPolicyStore(bool bNeedReport);  //周期性向策略中心发生设备状态

    void checkUdpIoConnection();      //通过是否周期性接收UDP IO消息，判断IO单板是否在线
    void heartbeatToUdpIo();          //周期向向UDP IO单板发送心跳消息
    void sendContinueKickPara(uint8_t continueKickEnable, uint8_t continueKickCount); //向UDP IO单板设置连续剔除参数
};

#endif // REALALARM_H
