#ifndef ENGINEPROJECT_H
#define ENGINEPROJECT_H

#include "IopAgent/RealAlarm.h"
#include "IopAgent/TcpClient.h"
#include "IopAgent/TcpImageReport.h"
#include "AssistLib/AuxTask.h"
#include "Multipler/ImageMultipler.h"
#include "Multipler/AbstractObject.h"
#include "IasAgent/AICtrl.h"
#include "IasAgent/channelInfer.h"

#define PRODUCT_TYPE_BOX           "box"
#define PRODUCT_YANSI_STOPMODE     "yansi_stopmode"

typedef std::function<ObjectPtr(int id, EngineProject *project)> ObjectCreator;

typedef enum ISG_IAS_TYPE_ENUM {
    IAS_DT = 0,
    IAS_AI = 1,
    IAS_DT_AI = 2
}IasTypeEnum;

typedef enum ISG_RUN_TYPE_ENUM {
    RunType_Infer = 0,  // 推理状态，图像可推理、可保存、可上报
//    RunType_Train,  // IAS采集图片训练阶段，图像不保存、不上报，仅使用在小包检测场景
//    RunType_Photo,  // 相机调试阶段，图像不推理、不保存，仅上报
    RunType_Spot = 1    // 点检，停止实体相机，启动软相机
}RunTypeEnum;

class EngineProject : public EngineObject
{
    Q_OBJECT

public:
    EngineProject();
    ~EngineProject();

public:
    void doCommand(const QString &cmd);                     // console界面，对Attribute进行赋值操作函数
    ObjectPtr findObject(const QString &objName) const;
    template <class T>
    static void registerObjectCreator(const QString &type); // 系统启动时，定义Json对象与ISG内部构造函数的对应关系

    // 获取配置文件中的参数
    QString productType() const { return m_productType.value().toString(); }
    int getAlgorithmType() const { return m_algorithmType.value().toInt(); }
    QString brandId() const { return QString::number(m_brandId.value().toInt()); }
    QString imageSavePath() const { return m_imageSavePath.value().toString(); }
    int get_ImageFileKeepDays() { return m_imageFileKeepDays.value().toInt(); }
    int get_LogFileKeepDays() { return m_logFileKeepDays.value().toInt(); }
    QString policyStore_Ip() const { return m_policyStoreIp.value().toString(); }
    int policyStore_port() const { return m_policyStorePort.value().toInt(); }
    int imageMaxInterval() const { return m_imageMaxInterval.value().toInt(); }
    int imageInferTimeout() const { return m_imageInferTimeoutMS.value().toInt(); }
    int objectInferTimeout() const { return m_objectInferTimeoutMS.value().toInt(); }
    int boxQueueLen() const { return m_boxQueueLen.value().toInt(); }
    int boxMinElapsed() const { return m_boxMinElapsed.value().toInt(); }

    // 全局类对象
    TcpClient *tcpClient() { return &m_tcpClient; }
    TcpImageReport *tcpImageReport() { return &m_tcpImageReport; }
    RealAlarm *realAlarm() { return &m_realAlarm; }
    QHash<QString, ObjectPtr> virtualCameras() { return m_virtualCameras; }
    QHash<int, AICtrl *> AICtrls() { return m_AICtrls; }
    QHash<int, ChannelInfer *> channelInfers() { return m_channelInfers; }
    QHash<int, CImageMultipler *> imageMultiplers() { return m_imageMultiplers;}
    AbstractObject *objectResult() { return m_objectResult; }

    // 设置或获取各种运行状态
    void setRunType(const RunTypeEnum runType); // 设置系统运行类型
    RunTypeEnum getRunType();                   // 获取系统运行类型
    void setRegistaredState(const bool state) { m_isRegistaredFlag = state;}        // 设置ISG注册状态
    bool getRegistaredState() { return m_isRegistaredFlag; }                        // 获取ISG注册状态
    void setPolicyStoreOnline(const bool state) { m_isPolicyStoreOnline = state;}   // 设置PolicyStore在线状态
    bool getPolicyStoreOnline() { return m_isPolicyStoreOnline; }                   // 获取PolicyStore在线状态

    AuxTask *m_pAuxTask;

protected:
    bool init();
    void cleanup();
    void onStarted();
    void onStopped();

private:
    bool openDef(const QString &path);          // 加载product_def.json文件，针对文件中的相机等每一个对象，调用loadDef
    bool loadDefNode(const QString &nodeName, const QJsonValue &json);  // 加载product_def.json中的某个对象
    bool loadParams(const QString &path);       // 加载product_params.json文件
    bool saveParams();                          // 保存product_params.json文件
    void loadAI();                              // 根据牌号目录下的AI算法目录，动态创建AI线程，设置AI关联的相机、加载AI文件
    void createVirtualCamera(const int &id);    // 创建点检需要的虚拟相机

    static QHash<QString, ObjectCreator> s_creator; // 对象字符串与内部构造函数的对应关系Hash表

private:
    Attribute   m_def;                          // product_def.json路径字符串
    Attribute   m_params;                       // product_params.json路径字符串
    Attribute   m_productType;                  // 产品类别
    Attribute   m_algorithmType;                // 算法类别
    Attribute   m_brandId;       	            // 当前牌号
    Attribute   m_save;                         // IOP向ISG下发命令,进行配置信息保存
    Attribute   m_imageSavePath;                // 图像及检测结果描述保存路径
    Attribute   m_imageFileKeepDays;            // 图像文件保存天数
    Attribute   m_logFileKeepDays;              // log文件保存天数
    Attribute   m_policyStoreIp;                // 设备作为pilocy store TCP客户端,定义地址
    Attribute   m_policyStorePort;              // 设备作为pilocy store TCP客户端,定义端口号
    Attribute   m_imageMaxInterval;             // 同一烟包多个相机拍摄的多幅图像的最大间隔
    Attribute   m_imageInferTimeoutMS;          // 图片检测超时时间
    Attribute   m_objectInferTimeoutMS;         // 推理超时时间
    Attribute   m_boxQueueLen;                  // 箱缺条产品，同时在跑道的烟箱数目
    Attribute   m_boxMinElapsed;                // 箱缺条产品，箱成形到读码的最小停留时间
    Attribute   m_originalImageReportRate;      // 原始图像上报比例
    Attribute   m_binningImageReportRate;       // 降分辨率图像上报比例

    TcpClient   m_tcpClient;                    // IOP作为TCP服务端，ISG作为TCP客户端
    TcpImageReport  m_tcpImageReport;           // IOP作为TCP服务端，ISG作为TCP客户端
    RealAlarm   m_realAlarm;                    // 告警模块

    QHash<QString, ObjectPtr> m_objects;       	// 对象Hash表
    QHash<QString, ObjectPtr> m_virtualCameras; // 点检虚拟相机对象Hash表

    QHash<int, AICtrl *> m_AICtrls;             // <ID，人工智能检测对象>Hash表
    QHash<int, ChannelInfer *> m_channelInfers; // <相机ID，传统算法处理对象>Hash表
    QHash<int, CImageMultipler *> m_imageMultiplers;    // <相机ID，图片结果汇总对象>Hash表
    AbstractObject *m_objectResult;             // 检测结果汇总对象

    RunTypeEnum m_runType;                  	// 当前运行状态
    bool        m_isRegistaredFlag;         	// 判断ISG是否注册成功
    bool        m_isPolicyStoreOnline;      	// 判断PolicyStore是否在线
};

inline ObjectPtr EngineProject::findObject(const QString &objName) const
{
    return m_objects.value(objName);
}

template <class T>
inline void EngineProject::registerObjectCreator(const QString &type)
{
    Q_ASSERT(!s_creator.contains(type));
    s_creator.insert(type, [](int id, EngineProject *project) {  //c++ Lambda表达式,用于定义并创建匿名的函数对象
        return std::make_shared<T>(id, project);
    });
}

#endif // ENGINEPROJECT_H
