#ifndef BARCODER_H
#define BARCODER_H

#include "BaseLib/EngineObject.h"
#include <QSerialPort>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

class BarCoder : public EngineObject
{
    Q_OBJECT

public:
    BarCoder(int id, EngineProject *project);
    ~BarCoder(void);
    virtual bool init() override;   //对象初始化函数
    void cleanup() override;  //对象清除函数
    bool isConnected() {return m_connected; }  //读码器串口是否打开成功
    static BarCoder *getInstance(); //获取读码器实例

private:
    static BarCoder *s_pInstance;

signals:
    void receiveCoder(const QByteArray codeData);

private slots:
    void BarCoderConnectAsync();  // 打开串口
    void BarCoderDisConnectAsync(); // 关闭串口
    void BarCoderReceiveBytes(); //串口readyRead信号的槽处理函数
    void onTimeout(); //关闭串口

private:
    QThread m_thread;
    QTimer   *m_timer;
    QSerialPort *m_serialPort;   //串口指针对象
    Attribute m_serialPortName;  //串口名称
    QByteArray m_readData;       //串口读出的数据缓存
    bool m_connected;            //读码器串口是否打开成功
};

#endif // SENSORCTRL_H
