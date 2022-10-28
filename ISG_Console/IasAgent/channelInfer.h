#ifndef CHANNELINFER_H
#define CHANNELINFER_H

#include "ias_type.h"
#include "iasJsonParser.h"
#include "../Inspector/Infer.h"
#include "IasBase/BaseLocator.h"
#include "IasBase/BaseDetector.h"

#include <QObject>
#include <QThread>
#include <QString>
#include <QHash>

class EngineProject;

#define DEFAULT_LOCATE_METHOD 0
#define DEFAULT_LOCATE_ID     0

class ChannelLocator
{
public:
    BaseLocator *m_baseLocator;
    int m_group;   //分组值，龙岩条包有左右通道特性，分成两组;
    int m_feature; //特征值，龙岩条包有左右通道特性，有特征匹配检测对象;
    int m_method;  //定位器类别：X定位器、Y定位器、Default定位器
    int m_id;      //定位器id
};

class ChannelDetector
{
public:
    BaseDetector *m_baseDetector;
    int m_group;   //分组值，龙岩条包有左右通道特性，分成两组;
    int m_feature; //特征值，龙岩条包有左右通道特性，有特征匹配检测对象;
    int m_method;  //检测器类别：直线检测、颜色检测等
    int m_id;      //检测器id
};

//传统算法检测，算法支撑对象
class ChannelInfer: public QObject
{
    Q_OBJECT

public:
    ChannelInfer(int channel_id, EngineProject *project);
    bool init();  //对象初始化
    void cleanup(); //对象清理
    bool loadAlgorithmConfig(const QString &algorithmJsonFilePath); //接收注册应答消息时，输入算法路径，加载算法json文件
    int getChannelId() { return m_channelID;}  //获取相机id
    int jsonChecksum() { return m_jsonChecksum; } //算法配置文件checksum

Q_SIGNALS:
    void imageIasResultSignal(const IMAGE_IAS_RESULT &imageIasResult);  //产生图像检测结果信号，ImageMultipler订阅该信号

private Q_SLOTS:
    bool initModel(); //加载算法
    void releaseModel(); //释放算法
    void onImageData(const IMAGE_DATA &imageData); //接收待检测的图像
    bool onOperateInfer(int op_type, int dt_type, const QJsonObject &inputJson, QJsonObject &resultJson); //IOP操作响应函数

private:
    void inferImage(const IMAGE_DATA &imageData); //图像检测
    BaseDTResult locate(int locator_id, const cv::Mat &mat); //定位
    BaseDTResult detect(int detector_id, const cv::Mat &mat, const QHash<int, BaseDTResult> &locator_results);//检测
    QByteArray resultJsonToByteArray(const QHash<int, BaseDTResult> &locator_results, const QHash<int, BaseDTResult> &detector_results, int group);
    int getAlgoChannelLocatorsNum(); //获取算法json脚本中detector数目
    int getAlgoChannelDetectorsNum(); //获取算法json脚本中locator数目
    QJsonObject getAlgoChannelLocator(int index); //获取算法json脚本中指定的locator
    QJsonObject getAlgoChannelDetector(int index); //获取算法json脚本中指定的detector

    bool getLocatePoint(const QJsonObject &inputJson, cv::Point2d &locatePoint); //获取静态配置的定位点
    bool getLocatePoint(int IdX, int IdY, cv::Point2d &locatePoint);  //获取静态配置的定位点
    bool addLocator(const QJsonObject &inputJson);
    bool addDetector(const QJsonObject &inputJson);
    bool removeLocator(const QJsonObject &inputJson);
    bool removeDetector(const QJsonObject &inputJson);
    bool updateLocator(const QJsonObject &inputJson);
    bool updateDetector(const QJsonObject &inputJson);
    bool resetLocator(const QJsonObject&inputJson, QJsonObject &resultJson); //将一个检测算法的配置恢复到算法推荐值，并返回
    bool resetDetector(const QJsonObject &inputJson, QJsonObject &resultJson); //将一个检测算法的配置恢复到算法推荐值，并返回
    bool execLocator(const QJsonObject &inputJson, QJsonObject &resultJson); //执行一个定位算法的处理，返回静态图检测结果
    bool execDetector(const QJsonObject &inputJson, QJsonObject &resultJson); //执行一个检测算法的处理，返回静态图检测结果
    /*
    脚本文件保存，计算文件的checksum，将checksum返回给IOP；
    脚本文件读取，计算文件的checksum，将checksum返回给IOP；
    IOP判断存储文件返回的checksum和读取文件的checksum是否一致，确认ISG的脚本文件与IOP的是否一致；
    */
    bool saveAlgorithmFile(const QJsonObject &jsonFileObj, int &checksum);
    int getAlgorithmFileChecksum(); //读算法配置文件，计算checksum

private:
    EngineProject *m_project;
    QThread m_thread;                       // 线程信息
    QHash<int, ChannelLocator *> m_locators;   // <定位算法id，算法对象> 定位算法的容器
    QHash<int, ChannelDetector *> m_detectors; // <检测算法id，算法对象> 检测算法的容器
    IasJsonParser *m_algorithmJsonParser;   // 算法配置文件JSon对象
    QString m_algorithmJsonFilePath;        // 算法配置路径
    QString m_algorithmJsonFileName;        // 算法配置文件名称
    int m_channelID;                        // 通道id
    bool m_bInitOK;                         // 加载成功标志
    bool m_bGroup;                          // 是否为左右通道检测
    int m_jsonChecksum;                     // 算法配置文件checksum
};

#endif // CHANNELINFER_H
