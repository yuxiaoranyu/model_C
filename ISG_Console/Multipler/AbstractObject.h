#ifndef ABSTRACT_OBJECT_H
#define ABSTRACT_OBJECT_H

#include "Inspector/Infer.h"
#include "BaseLib/EngineObject.h"
#include <QTimer>

class AbstractObject : public EngineObject {
    Q_OBJECT

public:
    explicit AbstractObject(EngineProject *project);
    virtual void preInfer(IMAGE_DATA &imageData) = 0;
    virtual void getStatistic(int &TotalCount, int &NgCount);
    void reportSet(int orginalReportRate, int binningReportRate);
    void originalReportRateSet(int orginalReportRate);
    void binningReportRateSet(int binningReportRate);
    bool orginalReport();
    bool binningReport();

protected:
    QAtomicInt m_detectTotalCount;    //检测对象总数，TCP线程会进行变量读取，将类别定义成QAtomic，以支持多线程访问
    QAtomicInt m_detectNgCount;       //检测缺陷对象总数，TCP线程会进行变量读取，将类别定义成QAtomic，以支持多线程访问

public:
    QAtomicInt m_originalReportRate;   // 原始图上报比例；
    int m_originalReportIndex;  // 原始图上报位置；
    QAtomicInt m_binningReportRate;    // 降分辨率图上报比例；
    int m_binningReportIndex;   // 降分辨率图上报位置；
};

#endif // ABSTRACT_OBJECT_H
