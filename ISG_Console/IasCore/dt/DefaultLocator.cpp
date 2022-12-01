#include "DefaultLocator.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

DefaultLocatorFactory defaultLocatorFactory;

void DefaultLocatorInit()
{
    RegisterBaseLocatorFactory(DEFAULT_LOCATE_METHOD, &defaultLocatorFactory);
}

bool DefaultLocator::load(const std::string &param)
{
    m_config = param;
    m_InitOK = true;
    m_run    = true;

    return true;
}

std::string  DefaultLocator::computeParam()
{
    return m_config;
}

BaseDTResult DefaultLocator::locate(const cv::Mat &mat)
{
    cv::Point localPoint = {0, 0};  //定位点
    BaseDTResult locateResult;
    QJsonObject resultJson;   //构建resultJson

    (void)mat;
    m_resultJson.insert("id", getId());
    m_resultJson.insert("method", DEFAULT_LOCATE_METHOD);
    m_resultJson.insert("dtType", DT_TYPE_LOCATOR);
    m_resultJson.insert("result", 1);

    locateResult.setId(getId());
    locateResult.setResult(true);
    locateResult.setLocatePoint(localPoint);
    locateResult.setPositionInfo(QJsonDocument(m_resultJson).toJson().toStdString());

    return locateResult;
}
