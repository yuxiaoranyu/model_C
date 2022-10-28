#ifndef IMAGEMULTIPLER_H
#define IMAGEMULTIPLER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include "BaseLib/EngineObject.h"
#include "BaseLib/Protocol.h"
#include "Inspector/Infer.h"

//图片检测记录表,用于维护图片的检测中间结果
struct IMAGE_RESULT_RECORD
{
    IMAGE_RESULT_RECORD() : result(RESULT_INIT){}
    IMAGE_DATA imageData;
    int         result;     // 检测结果
    QByteArray jsonData;    // 结果json
};

//图片检测结果数据结构，用于通知Object
struct OBJECT_IMAGE_RESULT {
    uint64_t objectId; // 对象id
    uint64_t imageId;  // 烟包ID
    int      result;   // 检测结果
};

class CImageMultipler : public EngineObject
{
    Q_OBJECT

public:
    CImageMultipler(int id, EngineProject *project);

    bool init() override; //初始化函数
    void cleanup() override; //清理函数
    void getStatistic(int &TotalCount, int &NgCount);

private:
    void sendImageResultDataToPolicyStore(const IMAGE_RESULT_RECORD &imageResult); // 图片与推理结果发送给policy store

signals:
    void imageResultSignal(const OBJECT_IMAGE_RESULT &objectImageResult);  //发送图片检测结果信号，ObjectResult订阅这个信号

private slots:
    void onImageData(const IMAGE_DATA &imageData);            //接收实体相机和点检相机图片数据，创建m_imageRecord数据，汇总图片检测结果时，需存在m_imageRecord数据
    void onImageIasResult(const IMAGE_IAS_RESULT &iasResult); //接收算法推理结果
    void onTimeout();  //超时资源回收

private:
    QThread m_thread;
    QTimer *m_timer;

    uint64_t m_inferTimeout;         //图片检测超时时间，默认为200ms
    QHash<uint64_t, IMAGE_RESULT_RECORD> m_imageResultRecord;  ///< 图像ID，图像检测结果> 图像检测记录
    QHash<uint64_t, uint32_t> m_imageIasIdRecord;     ///< 图像ID，算法ID> 如果有多个算法参与检测，记录已返回结果的算法数

    uint32_t   m_IasCount;            //参与检测的算法数目
    QAtomicInt m_detectTotalCount;    //检测总数，TCP线程会进行变量读取，将类别定义成QAtomic，以支持多线程访问
    QAtomicInt m_detectNgCount;       //缺陷总数，TCP线程会进行变量读取，将类别定义成QAtomic，以支持多线程访问
};

#endif // IMAGEMULTIPLER_H
