#include "Detector.h"
#include "DtCommon.h"
#include "IasCore/dt/api/lines.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

Detector::Detector() :
    m_run(true),
    m_InitOK(false),
    m_locate_x_id(0),
    m_locate_y_id(0)
{
}

Detector::~Detector()
{
}

bool Detector::update(const std::string &param)
{
    return load(param, m_locate_root);
}

bool Detector::remove()
{
    return true;
}

bool Detector::exec(std::string &result)
{
    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg",  cv::IMREAD_UNCHANGED);

    detect(matRead, m_locate_root);

    result = QJsonDocument(m_resultJson).toJson().toStdString();

    return true;
}

bool Detector::resetParam(std::string &result)
{
    result = computeParam();

    return true;
}

int Detector::getLocateIdX()
{
    return m_locate_x_id;
}

int Detector::getLocateIdY()
{
    return m_locate_y_id;
}

void Detector::setLocateRoot(const cv::Point2d &locate_point)
{
    m_locate_root = locate_point;
}

cv::Rect2d Detector::getModifiedRect(const cv::Point2d &locate_point)
{
    cv::Rect2d rect_roi;

    if (getLocateIdX() == 0 && getLocateIdY() == 0)
    {
        rect_roi = m_rect_roi;
    }
    else
    {
        rect_roi =  m_rect_roi + (locate_point - m_locate_root);
    }

    return rect_roi;
}
