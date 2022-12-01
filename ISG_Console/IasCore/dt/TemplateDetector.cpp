#include "TemplateDetector.h"
#include "DtCommon.h"
#include "../common/GenJsonObject.h"
#include "IasCore/dt/api/lines.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

TemplateDetectorFactory templateDetectorFactory;

void TemplateDetectorInit()
{
    RegisterBaseDetectorFactory(DETECT_METHOD_TMEMPLATE, &templateDetectorFactory);
}

bool TemplateDetector::getMatchTemplate(const cv::Mat &frame, double result_data[9])
{
    cv::Mat result_img;
    double min_value, max_value;
    cv::Point min_location, max_location;

    //模板匹配
    cv::matchTemplate(frame, template_img, result_img, cv::TM_CCOEFF_NORMED);

    //输入图像，获取图像的最小值、最大值、最小值坐标、最大值坐标
    cv::minMaxLoc(result_img, &min_value, &max_value, &min_location, &max_location, cv::Mat());

    result_data[0] = max_value;
    result_data[1] = max_location.x;  //左上角
    result_data[2] = max_location.y;
    result_data[3] = max_location.x + template_img.cols;  //右上角
    result_data[4] = max_location.y;
    result_data[5] = max_location.x + template_img.cols; //右下角
    result_data[6] = max_location.y + template_img.rows;
    result_data[7] = max_location.x;                      //左下角
    result_data[8] = max_location.y + template_img.rows;

    return (max_value > matching_score_threshold) ? true : false;
}

bool TemplateDetector::load(const std::string &param, const cv::Point2d& locate_point)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    m_config = param;
    m_InitOK = false;

    m_run = jsonDocObj.value("run").toBool(true);

    m_locate_x_id = jsonDocObj.value("locator_x_id").toInt(0);
    m_locate_y_id = jsonDocObj.value("locator_y_id").toInt(0);

    setLocateRoot(locate_point);
    matching_score_threshold = jsonDocObj.value("matching_score_threshold").toDouble();

    QJsonArray pointsArray = jsonDocObj.value("roi").toArray(); //加载检测区域
    if (pointsArray.size() == 2)
    {
        cv::Point p1, p2;

        QJsonObject p1JsonObj = pointsArray[0].toObject();
        p1.x = p1JsonObj.value("x").toInt();
        p1.y = p1JsonObj.value("y").toInt();

        QJsonObject p2JsonObj = pointsArray[1].toObject();
        p2.x = p1JsonObj.value("x").toInt();
        p2.y = p1JsonObj.value("y").toInt();

        m_rect_roi = cv::Rect(p1, p2);
    }
    else
    {
        qInfo() << "TemplateDetector: roi is error";
        return false;
    }

    QJsonArray pointDataArray = jsonDocObj.value("point_data").toArray();
    cv::Point2d line_point[4];

    for (int i = 0; i < pointDataArray.size(); i++)
    {
        QJsonObject pointJsonObj = pointDataArray[i].toObject();

        line_point[i].x = pointJsonObj.value("x").toInt();
        line_point[i].y = pointJsonObj.value("y").toInt();

        if (i >= 3)
        {
            break;
        }
    }

    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg");
    if (matRead.empty())
    {
        qInfo() << "TemplateDetector: can not read channel.jpg";
        return false;
    }
    else
    {
        cv::Rect2d template_roi(line_point[0].x, line_point[0].y, line_point[1].x - line_point[0].x, line_point[3].y - line_point[0].y);

        template_img = matRead(template_roi);
        if (template_img.empty())
        {
            qInfo() << "TemplateDetector: point_data is not correct";
            return false;
        }
    }

    m_InitOK = true;

    return true;
}

std::string TemplateDetector::computeParam()
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(m_config).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    matching_score_threshold = 0.8;
    jsonDocObj.insert("matching_score_threshold", 0.8);

    m_config = QString(QJsonDocument(jsonDocObj).toJson()).toStdString();

    return m_config;
}

BaseDTResult TemplateDetector::detect(const cv::Mat &frame, const cv::Point2d& locate_point)
{
    QJsonObject resultJson;    //结果json
    bool result = false;

    QJsonObject drawPolygonJson;  //多边形Json
    QJsonObject drawRectJson;
    if (m_run && m_InitOK)
    {
        cv::Mat img_roi;
	    cv::Rect2d rect_roi;

        rect_roi = getModifiedRect(locate_point);
        double result_data[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

        try
        {
            img_roi = (frame)(rect_roi);
        }
        catch (const std::exception &e)
        {
            cv::Rect2d all_roi(0, 0, frame.cols, frame.rows);

            rect_roi = rect_roi & all_roi;  //矩形交集
            img_roi = (frame)(rect_roi);
        }

        if (!img_roi.empty())
        {
            result = getMatchTemplate(img_roi, result_data);
        }

        resultJson.insert("matching_score", result_data[0]);

        QVector<QJsonObject> polygonPoint;
        polygonPoint.append(genPointJson(result_data[1] + rect_roi.x, result_data[2] + rect_roi.y));
        polygonPoint.append(genPointJson(result_data[3] + rect_roi.x, result_data[4] + rect_roi.y));
        polygonPoint.append(genPointJson(result_data[5] + rect_roi.x, result_data[6] + rect_roi.y));
        polygonPoint.append(genPointJson(result_data[7] + rect_roi.x, result_data[8] + rect_roi.y));

        QVector<QJsonObject> rectPoint;
        rectPoint.append(genPointJson(rect_roi.x, rect_roi.y));
        rectPoint.append(genPointJson(rect_roi.br().x, rect_roi.br().y));

        if (result)
        {
            drawPolygonJson = genDrawObjJson("polygon", polygonPoint, DRAW_COLOR_IKB);
            drawRectJson = genDrawObjJson("rect", rectPoint, DRAW_COLOR_GREEN);
        }
        else
        {
            drawPolygonJson = genDrawObjJson("polygon", polygonPoint, DRAW_COLOR_RED);
            drawRectJson = genDrawObjJson("rect", rectPoint, DRAW_COLOR_RED);
        }
        resultJson.insert("draw", genDrawJson(drawRectJson, drawPolygonJson));
    }
    else
    {
        result = true;
    }

    resultJson.insert("id", getId());
    resultJson.insert("method", DETECT_METHOD_TMEMPLATE);
    resultJson.insert("dtType", DT_TYPE_DETECTOR);
    resultJson.insert("result", result ? 1 : 0);

    m_resultJson = resultJson;

    BaseDTResult detectResult;
    detectResult.setId(getId());
    detectResult.setResult(result);
    detectResult.setPositionInfo(QJsonDocument(m_resultJson).toJson().toStdString());

    return detectResult;
}
