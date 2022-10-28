#ifndef TCPREPORT_H
#define TCPREPORT_H

#include "IOCtrl/PCPUdpCtrl.h"
#include "BaseLib/EngineObject.h"
#include "BaseLib/Protocol.h"

#include <QThread>
#include <QJsonDocument>
#include "Multipler/ImageMultipler.h"

#define MAX_IMG_REPORT_QUEUE_SIZE       10   // 等待图像上报的最大排队消息个数

class QTcpSocket;
class QTimer;
class ProtocolStream;

// 与策略中心的TCP连接,发送图片数据线程
class TcpImageReport : public EngineObject
{
    Q_OBJECT

private:
    enum ImageMediaReportType
    {
        IMAGE_REPORT_NULL     = 0,  //不上报
        IMAGE_REPORT_ORIGINAL = 1,  //上报原始图片
        IMAGE_REPORT_BINNING  = 2,  //上报降分辨率图片
        IMAGE_REPORT_BOTH     = 3   //上报原始图片+降分辨率图片
    };

public:
    explicit TcpImageReport(EngineProject *project);
    void establisthTcpReport(QString hostName, quint16 hostPort); // 注册响应阶段，根据策略中心传递的媒体服务器地址，触发TCP建立
    void sendCreateImageReport(); //媒体通道TCP连接建立后，向Java图像子系统发送媒体通道创建命令
    void sendImageResultToPolicyStore(const IMAGE_RESULT_RECORD &imageRecord);  // 向策略中心发送图片数据及检测结果
    void sendImageDataToPolicyStore(const IMAGE_DATA &imageData); // 向策略中心发送图片数据
    void sendBarCodeToPolicyStore(const uint64_t objectId, const QByteArray &barCode); //箱缺条产品，向策略中心上报一号工程码

protected:
    void cleanup();

private slots:
    void onTimeout(); //超时
    void onConnected(); //TCP连接建立
    void onDisconnected(); //TCP连接断开
    void onMessageCTRL(const POLICY_INFO_MSG_HEADER &head, const QByteArray &body); //接收TCP消息
    void sendResponse(const POLICY_INFO_MSG_HEADER &head, const QJsonObject &jsonObj); //回应TCP命令
    void sendImageResultToPolicyStoreAsync(const IMAGE_RESULT_RECORD &imageRecord); // 向策略中心发送图片数据及检测结果
    void sendImageDataToPolicyStoreAsync(const IMAGE_DATA &imageData); // 向策略中心发送图片数据
    void sendBarCodeToPolicyStoreAsync(const uint64_t objectId, const QByteArray &barCode); //箱缺条产品，向策略中心上报一号工程码

private:
    QThread        m_thread;           // 独立线程
    QString        m_hostName;         // TCP Server地址
    quint16        m_hostPort;         // TCP Server端口
    QTcpSocket     *m_socket;          // TCP Socket
    ProtocolStream *m_stream;          // TCP Socket数据流接收、发送封装
    QTimer         *m_timer;           // TCP连接断线重连定时器
    bool            m_isOnline;        // TCP连接状态
    int             m_mediaReportType; // 0-不上报 1-原图；2-压缩图；3-原图+压缩图
    QAtomicInt      s_reportQueuedCount;  // 排队等待上报的消息数;超过阈值,丢掉消息
    uint64_t        m_createReportMsgTransId;  // 通道创建消息事务ID
    bool            m_bCleaned;        // 系统退出的clean标志
};

#endif // TCPREPORT_H
