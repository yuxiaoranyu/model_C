#include "AbstractObject.h"
#include "BaseLib/EngineProject.h"
#include <QTimer>
#include <QDebug>

AbstractObject::AbstractObject(EngineProject *project) :
    EngineObject(project),
    m_detectTotalCount(0),
    m_detectNgCount(0),
    m_originalReportRate(1),
    m_originalReportIndex(0),
    m_binningReportRate(3),
    m_binningReportIndex(0)       // 降分辨率图上报位置；
{
}

void AbstractObject::getStatistic(int &TotalCount, int &NgCount)
{
    TotalCount = m_detectTotalCount;
    NgCount = m_detectNgCount;

    m_detectTotalCount = 0;
    m_detectNgCount = 0;
}

void AbstractObject::originalReportRateSet(int originalReportRate)
{
    m_originalReportRate = originalReportRate;
}

void AbstractObject::binningReportRateSet(int binningReportRate)
{
    m_binningReportRate = binningReportRate;
}

bool AbstractObject::orginalReport()
{
    if (m_originalReportRate <= 0) {
        return false;
    }

    m_originalReportIndex ++;
    if (m_originalReportIndex >= m_originalReportRate) {
        m_originalReportIndex = 0;
        return true;
    } else {
        return false;
    }
}

bool AbstractObject::binningReport()
{
    if (m_binningReportRate <= 0) {
        return false;
    }

    m_binningReportIndex ++;
    if (m_binningReportIndex >= m_binningReportRate) {
        m_binningReportIndex = 0;
        return true;
    } else {
        return false;
    }
}
