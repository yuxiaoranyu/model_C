#ifndef BOXRESULT_H
#define BOXRESULT_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include "BaseLib/EngineObject.h"
#include "Inspector/Infer.h"
#include "ImageMultipler.h"
#include "AbstractObject.h"

struct BoxObject_S {   //烟箱对象
    uint64_t    objectId;         // 烟箱ID
    bool        bReportOrignal;   // 原图和检测结果上报IOP
    bool        bReportBinning;   // Binning图和检测结果上报IOP
    uint64_t    generateTime;     // 烟箱创建时间
    bool        result;           // 检测结果 true：无错误 false：有错误
    QByteArray  barCode;          // 烟箱一号工程码
    QHash<uint64_t, uint32_t> imageCameraIdHash; // <imageID, cameraId>烟包中的各个图片的相机信息
    QHash<uint64_t, uint16_t> imageResultHash;   // <imageID, Result>烟包中的各个图片的结果信息
};

class CBoxResult : public AbstractObject
{
    Q_OBJECT

public:
    CBoxResult(EngineProject *project);

    bool init() override;       // 初始化函数
    void cleanup() override;    // 清除函数
    void preInfer(IMAGE_DATA &imageData) override;  // 烟箱管理

private slots:
    void preInferAsync(IMAGE_DATA &imageData);      // 烟箱管理
    void onBarCode(const QByteArray barCode);       // 接收一号工程码槽
    void onImageResult(const OBJECT_IMAGE_RESULT &objectImageResult);   // 接收图片检测结果

private:
    void enqueue(qint64 currTime);          // 烟箱加入队列
    bool dequeue(const QByteArray barCode); // 烟箱移出队列
    void sendBarCodeInfoToPolicyStore();    // 将一号工程码及相关的imageId通知策略中心
    BoxObject_S *findBox(const uint64_t objectId);  // 输入ObjectId，查找烟箱对象
    int findCameraId(BoxObject_S *boxPtr, const uint64_t imageId);  // 输入烟箱对象和imageId，查找cameraId

private:
    QThread  m_thread;
    bool     m_useBarCoder;  // 是否配置了读码器

    /*
        烟箱列表用于ObjectId管理，将一号工程码与箱成形、堆垛、外观图片关联；
        烟箱列表映射错误会引发图片的ObjectId错误、图片无法使用一号工程码进行重命名；
        读码丢失引发烟箱列表映射错误没有解决方案，通过队列机制，自动恢复；
    */
    int         m_queueLen;    // 流水线上的烟包数，不同烟厂不一样，要么为2，要么为3
    QList<BoxObject_S> m_box_queue;  // 流水线上的烟箱列表
    int         m_minElapsed;  // 箱成形到读码的最小停留时间

    bool        m_boxFoundFromQueue; //队列中找到烟箱数据
    BoxObject_S m_boxPoped;  // 从队列中移出的烟箱

    bool        m_isWaiguanFirst;  // 箱外观图片成组，针对一个烟箱，指明是否是箱外观的首张图片
    qint64      m_WaiguanFirstTime;  // 箱外观图片成组，针对一个烟箱，指明箱外观首张图片的时间
};

#endif // BOXRESULT_H
