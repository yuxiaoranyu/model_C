#ifndef PCPUDPCTRL_H
#define PCPUDPCTRL_H

#include "BaseLib/EngineObject.h"
#include <QThread>
#include <QObject>
#include <QWaitCondition>

#define PCP_UDP_MAGIC           0x55AA55AA  // PCP_UDP消息魔术字

class QUdpSocket;

#pragma pack(1)

struct PCP_UDP_MSG_HEAD //与PCP_UDP单板通信的帧头
{
    uint32_t magic;             // 消息魔术字，固定为0x55AA55AA
    uint16_t uid;               // 事务id，区分不同的消息，每发送一个消息id加1
    uint16_t cmd;               // 消息命令字，表项下发定义为1，回应消息定义为100，可扩充
    uint32_t msgLen;            // 消息体长度，不包括消息头
};

struct PCP_UDP_MSG_PacketProduct_Result_Body //小包、条包检测结果消息体
{
    uint32_t genSecond;         // 时间戳(秒)
    uint32_t genMillisecond;    // 时间戳(毫秒)
    uint8_t  result;            // 剔除结果
};

typedef struct PCP_UDP_VERSION_BODY //PCP_UDP单板通信消息体
{
    char moduleName[16];        // 单板类别
    char moduleVersion[16];     // 版本号
    char publishDate[16];       // 构建时间
}PcpUdpVersionBody;

#pragma pack()

class PCPUdpCtrl : public EngineObject
{
    Q_OBJECT

public:
    enum UDP_CMD_TYPE_ENUM
    {
        UdpCmd_Kick                 = 1,    ///< 发送剔除命令
        UdpCmd_KickEnable           = 2,    ///< 剔除使能,单个剔除
        UdpCmd_KickDisable          = 3,    ///< 剔除关闭,单个剔除
        UdpCmd_AbnormalKickEnable   = 4,    ///< 光纤信号异常连续剔除使能
        UdpCmd_AbnormalKickDisable  = 5,    ///< 光纤信号异常连续剔除关闭
        UdpCmd_TriggerCameraSNAP    = 6,    ///< 触发相机拍照
        UdpCmd_RunStart             = 7,    ///< 通知下位机开始运行
        UdpCmd_RunStop              = 8,    ///< 通知下位机停止运行
        UdpCmd_Reboot               = 9,    ///< 下位机通知上位机重新启动
        UdpCmd_Kick_Statistic       = 10,   ///< 下位机通知上位机剔除计数
        UdpCmd_SetKickDelay         = 11, 	///< 修改踢除延时
        UdpCmd_InquireVersionInfo   = 12,   ///< 查询小板子的版本信息
        UdpCmd_Box_Shape_Sigal      = 13,   ///< 箱缺条产品箱体成形信号
        UdpCmd_Box_Stop_Machine1    = 14,   ///< 箱缺条产品箱成形错误/堆垛错误停机
        UdpCmd_Box_Stop_Machine2    = 15,   ///< 箱缺条产品箱外观错误停机
        UdpCmd_PcpAck               = 199,  ///< 命令字应答
        //--------------------- 告警项下发给PCP ----------------------
        UdpCmd_ContinueKickParaPush = 200,  ///< 告警参数下发
        UdpCmd_ContinueKickStart,           ///< 连剔开
        UdpCmd_ContinueKickBreak,           ///< 连剔中断
        UdpCmd_LedGreen,                    ///< 亮绿灯
        UdpCmd_LedRed,                      ///< 亮红灯
        //--------------------- 告警项PCP上报 ----------------------
        UdpCmd_HeardBeat = 210,             ///< 心跳点，表示所有状态均正常
        UdpCmd_KickStateNg,                 ///< 剔除异常
        UdpCmd_BoxFull,                     ///< 满箱
    };
    Q_ENUM(UDP_CMD_TYPE_ENUM)

public:
    PCPUdpCtrl(int id, EngineProject *project);
    static PCPUdpCtrl *getInstance();
    virtual bool init() override;
    //重写基类的虚函数
    void onStarted() override; //启停函数
    void onStopped() override; //启停函数
    void cleanup() override;

public:
    void sendData(const uint16_t cmd, const QByteArray &buffer);
    qint64 heartbeatReceivedTime() { return m_heartbeatLastTime; }
    void getNGCount(int &ngCount); //读清方式获取IO记录的剔除数目
    QStringList getPcpVersionInfo() {return m_pcpVersionInfoList;}  //获取pcp的版本号
    bool getKickEnable() {return m_kickEnable.value().toBool();}

signals:
    void sendDataToRealAlarm(const PCP_UDP_MSG_HEAD &head); //接收IO板告警相关消息，发送信号，通知告警模块
    void boxShapeSignal(); //箱体成形信号

private slots:
    void onReadyRead();        //UDP Socket接收消息
    void sendDataAsync(const uint16_t cmd, const QByteArray &buffer);

private:
    void setKickEnabled(bool enabled);
    void setKickAbnormalEnabled(bool enabled);

    static PCPUdpCtrl *s_pInstance;

private:
    QThread    m_thread;                // pcp udp 线程
    QUdpSocket *m_socket;

    Attribute  m_udpLocalIp;            // 本地地址
    Attribute  m_udpLocalPort;          // 本地端口号
    Attribute  m_udpRemoteIp;           // 目的地址
    Attribute  m_udpRemotePort;         // 目的端口号
    Attribute  m_kickEnable;            // 是否使能剔除
    Attribute  m_kickAbnormalEnable;    // 是否使能异常连续剔除

    QStringList m_pcpVersionInfoList;   // 小板子的版本信息<board_type, version, compile_time>
    qint64     m_heartbeatLastTime;     // 接收小板子的心跳时间
    QAtomicInt m_kickCount;             // 小板子统计的剔除数目
};

#endif // PCPUDPCTRL_H
