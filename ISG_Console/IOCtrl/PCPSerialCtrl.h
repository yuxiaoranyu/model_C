#ifndef PCPSERIALCTRL_H
#define PCPSERIALCTRL_H

#include "BaseLib/EngineObject.h"
#include <QSerialPort>
#include <QThread>
#include <QWaitCondition>

#define MAX_PCP_MSG_BUFFER_SIZE 128       //发送消息允许的最大长度
#define COMAND_SET_BATCH_ATTRIBUTE_VAL 19 //PCP批量属性设置标志位
#define UART_PREAMBLE_CODE 0x55AA         //与PCP单板通信的前导码

#pragma pack(1)
typedef struct T_UART_FRAME_HEAD //与PCP单板通信的帧头
{
    unsigned short preamble; //16位前导码,0xAA55
    unsigned short len;      //帧长度，包括帧头
    unsigned char CRC;       //CRC，简单和
    unsigned char reserve;
    unsigned char Data[2]; //数据信息
} UART_FRAME_HEAD;

typedef struct tagPCP_MSG_HEAD //与PCP单板通信的业务消息头
{
    unsigned int MsgLen;      //消息总长度,4字节倍数
    unsigned short FrameLen;  //本帧的长度，未使用
    unsigned char MsgCode;    //命令码
    unsigned char DataSegNum; //数据段数目

    union
    {
        int iVal;
        unsigned int dwVal;
        struct
        {
            unsigned char ReturnCode;
            unsigned char Reserved;
        };
        struct
        {
            unsigned char OpCode;
            unsigned char Index;
            unsigned char CommType;
        };
    };
} PCP_MSG_HEAD;
#pragma pack()

class PCPSerialCtrl : public EngineObject
{
    Q_OBJECT

public:
    enum Result
    {
        OK,
        NG,
        Timeout
    };

public:
    PCPSerialCtrl(int id, EngineProject *project);
    ~PCPSerialCtrl(void);
    static PCPSerialCtrl *getInstance();
    virtual bool init(); //设备启动，配置恢复完成后，底层调用各对象的init，通知各对象做自身特有的初始化

    void PCP_KickSendData(); //提供给外部的数据发送接口
    void TriggerCameraSnap(); //触发相机拍照

private:
    static PCPSerialCtrl *s_pInstance;

private slots:
    void PCP_SendDataAsync(const QString &comName, const QString &valString); //向串口线程注册的数据发送响应槽
    void PCP_ReceiveBytes();  //向串口注册的readyRead()事件接收槽

    void IoConnectAsync();    //向串口线程注册的串口连接命令响应槽
    void IodisConnectAsync(); //向串口线程注册的串口断开连接命令响应槽

private:
    void IoConnect();                                                             //建立PCP连接
    void IodisConnect();                                                          //关闭PCP连接
    bool IoSendData(const char *buf, const int len);                                          //PCP底层通信函数
    void onMessagePCP_Ack(const PCP_MSG_HEAD &msgHeader, const QByteArray &body); //PCP返回消息处理

private:
    QThread m_thread;                   // PCP通信线程
    Attribute m_kickEnable;             // 是否使能剔除

    Attribute m_serialPortName;         // PCP连接时，使用的串口
    Attribute m_attributeAdress;        // PCP剔除，使用的属性地址
    Attribute m_attributeValue;         // PCP剔除，PCP写入值
    Attribute m_snapAttributeAdress;    // PCP拍照触发，使用的属性地址
    Attribute m_snapAttributeValue;     // PCP拍照触发，PCP写入值

    QSerialPort *m_serialPort;          // 串口对象需在PCP线程中定义，因此定义成指针形式

};

#endif // PCPSERIALCTRL_H
