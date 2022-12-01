#include "Locator.h"
#include "DtCommon.h"
#include "IasCore/dt/api/lines.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

Locator::Locator() :
    m_run(true),
    m_InitOK(false)
{
}

Locator::~Locator()
{
}

bool Locator::update(const std::string &param)
{
    return load(param);
}

bool Locator::resetParam(std::string &result)
{
    result = computeParam();

    return true;
}

bool Locator::exec(std::string &result)
{
    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg");
    locate(matRead);

    result = QJsonDocument(m_resultJson).toJson().toStdString();

    return true;
}

bool Locator::remove()
{
    return true;
}

void Locator::setLocateRoot(const cv::Point2d &locate_root)
{
    m_locate_root = locate_root;
}

cv::Point2d Locator::getLocateRoot()
{
    return m_locate_root;
}
