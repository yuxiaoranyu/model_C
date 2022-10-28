#ifndef IMAGEGROUP_H
#define IMAGEGROUP_H

#include <QThread>
#include <QTimer>
#include <QHash>
#include "../Inspector/Infer.h"

class EngineProject;
class AICtrl;

//AI推理，多幅图片组成一组，一次性提交给AI，提升检测速度
//支持多组推理，每个实例关注该组对应的相机，订阅该组相机涉及的图片
class ImageGroup: public QObject
{
    Q_OBJECT

public:
    ImageGroup(int id, EngineProject *project, AICtrl *pAICtrl);
    bool init();    // 初始化，创建线程和定时器
    void cleanup(); // 对象清除函数
    void addCameraId(int cameraId) { m_cameraIdList.append(cameraId); }
    void setBatch(bool bBatch) { m_Batch = bBatch; }

Q_SIGNALS:
    void imageGroupSignal(const IMAGE_GROUP_DATA &imageGroup); // 产生图像成组信号，BatchInfer会订阅这个信号

private slots:
    void onImageData(const IMAGE_DATA &imageData); //接收待算法处理的图像，进行成组处理。一般，一个烟包的多张图片组成一组
    void onTimeout(); //成组超时扫描，成组超时的图片，不再等待，需及时提交AI推理

private:
    EngineProject *m_project;
    AICtrl *m_AICtrl;
    QThread m_thread; //成组管理线程
    QTimer* m_timer;  //周期性扫描定时器，扫描超时group，针对超时group，产生图像成组信号
    int     m_channelID;                   // 通道id
    QList<int> m_cameraIdList; //组相机列表
    bool    m_Batch;
    QHash<uint64_t, IMAGE_GROUP_DATA> m_groupHash; //<烟包id，烟包的多幅图片> 组管理Hash表
};

#endif // IMAGEGROUP_H
