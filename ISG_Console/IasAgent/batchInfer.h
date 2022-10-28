#ifndef BATCHINFER_H
#define BATCHINFER_H

#include <QObject>
#include <QThread>
#include <QString>
#include "iasJsonParser.h"
#include "IasBase/BaseAiInference.h"
#include "ias_type.h"
#include "../Inspector/Infer.h"

class EngineProject;
class AICtrl;

//AI检测，算法支撑对象
//支持多组推理，可对该对象进行多实例，每个实例关注该组对应的图片，进行Batch推理
class BatchInfer: public QObject
{
    Q_OBJECT

public:
    BatchInfer(int id, EngineProject *project, AICtrl *pAICtrl); //构造函数

    bool init(); //对象初始化
    void cleanup(); //对象清理
    void addCameraId(int cameraId) { m_cameraIdList.append(cameraId); }
    QList<int> getCarmeraIdList() { return m_cameraIdList; }
    int getChannelId() { return m_channelID;}  //获取通道id
    bool loadAlgorithmConfig(const QString &algorithmJsonFilePath); //接收注册应答消息时，输入算法路径，加载算法json文件
    int jsonChecksum() { return m_jsonChecksum; }

Q_SIGNALS:
    void imageIasResultSignal(const IMAGE_IAS_RESULT &imageIasResult);  //产生图像检测结果信号，ImageMultipler订阅该信号

private slots:
    bool initModel(); //加载模型
    void releaseModel(); //释放模型
    void onImageGroup(const IMAGE_GROUP_DATA &imageGroup);  //接收成组的待检测图像，进行AI检测
    bool onOperateInfer(int op_type, const QJsonObject &inputJson, QJsonObject &resultJson); //IOP操作响应函数

private:
    QVector<QByteArray> resultVectorToByteArray(const std::vector<int> &cameraIdVector, const std::vector<BaseAIResult> &resultVector);//构建检测结果Json
    bool addAiInference(const QJsonObject &inputJson);  //加载模型
    /*
    脚本文件保存，计算文件的checksum，将checksum返回给IOP；
    脚本文件读取，计算文件的checksum，将checksum返回给IOP；
    IOP判断存储文件返回的checksum和读取文件的checksum是否一致，确认ISG的脚本文件与IOP的是否一致；
    */
    bool saveAlgorithmFile(const QJsonObject &jsonFileObj, int &checksum);
    int getAlgorithmFileChecksum(); //读算法配置文件，计算checksum

private:
    EngineProject *m_project;
    AICtrl *m_AICtrl;
    QThread m_thread;
    BaseAiInference *m_aiInference;        // ai推理对象指针
    QList<int> m_cameraIdList;             // 组相机列表
    IasJsonParser *m_algorithmJsonParser;  // 算法配置文件JSon对象
    QString m_algorithmJsonFilePath;       // 算法配置路径
    QString m_algorithmJsonFileName;       // 算法配置文件名称
    int m_channelID;                       // 通道id
    bool m_bInitOK;                        // ai对象加载成功标志
    int m_jsonChecksum;                    // 算法配置文件checksum
};

#endif // BATCHINFER_H
