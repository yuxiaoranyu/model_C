#include "LineDetector.h"
#include "DtCommon.h"
#include "../common/GenJsonObject.h"
#include "IasCore/dt/api/lines.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

LineDetectorFactory lineDetectorFactory;

void LineDetectorInit()
{
    RegisterBaseDetectorFactory(DETECT_METHOD_HOUGH_LINE, &lineDetectorFactory);
}

bool LineDetector::getLine(const cv::Mat &frame, double result_data[7])
{
    ias::feature::Line line(10, 0,
                                ias::feature::HOUGH_SCORE,
                                ias::feature::LINE_METHOD_2,
                                line_angle_range, line_length_range * line_length);

    line.setBinary_Threshold_(binary_threshold);
    auto find_lines_data = line.Find(frame);

    if (find_lines_data.size() >= 1)
    {
        result_data[0] = find_lines_data[0].score;
        result_data[1] = find_lines_data[0].pt1.x;
        result_data[2] = find_lines_data[0].pt1.y;
        result_data[3] = find_lines_data[0].pt2.x;
        result_data[4] = find_lines_data[0].pt2.y;
        result_data[5] = find_lines_data[0].angle_diff;
        result_data[6] = 1;

        if (find_lines_data[0].score > line_score_threshold)
        {
            return true;
        }
    }

    return false;
}

bool LineDetector::load(const std::string &param, const cv::Point2d& locate_point)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    m_config = param;
    m_InitOK = false;

    m_run = jsonDocObj.value("run").toBool(true);

    m_locate_x_id = jsonDocObj.value("locator_x_id").toInt(0);
    m_locate_y_id = jsonDocObj.value("locator_y_id").toInt(0);

    setLocateRoot(locate_point);

    line_length_range = jsonDocObj.value("line_length_range").toDouble();
    binary_threshold = jsonDocObj.value("binary_threshold").toDouble();
    line_score_threshold = jsonDocObj.value("line_score_threshold").toDouble();

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
        qInfo() << "LineDetector: roi is error";
        return false;
    }

    QJsonArray pointDataArray = jsonDocObj.value("point_data").toArray();
    if (pointDataArray.size() == 2)
    {
        cv::Point p1, p2;

        QJsonObject p1JsonObj = pointDataArray[0].toObject();
        line_point[0].x = p1JsonObj.value("x").toInt();
        line_point[0].y = p1JsonObj.value("y").toInt();

        QJsonObject p2JsonObj = pointDataArray[1].toObject();
        line_point[1].x = p1JsonObj.value("x").toInt();
        line_point[1].y = p1JsonObj.value("y").toInt();

        if (line_point[0].y > line_point[1].y)
        {
            swap(line_point[0], line_point[1]);
        }
    }

    line_length = ias::feature::LineTool::getLineLength(line_point[0], line_point[1]);
    line_angle_range[0] = ias::feature::LineTool::getLineAngle(line_point[1], line_point[0]);;
    line_angle_range[1] = jsonDocObj.value("angle_range").toDouble();

    if ((line_length_range < 0) || (line_length_range > 1))
    {
        qInfo() << "LineDetector: line_length_range is out of range";
        return false;
    }

    if ((binary_threshold < 0) || (binary_threshold > 256))
    {
        qInfo() << "LineDetector: binary_threshold is out of range";
        return false;
    }

    m_InitOK = true;

    return true;
}

std::string LineDetector::computeParam()
{
    return m_config;
}

BaseDTResult LineDetector::detect(const cv::Mat &frame, const cv::Point2d& locate_point)
{
    bool result = false;   //定位结果
    double result_data[7] = {0, 0, 0, 0, 0, 0, 1};
    cv::Rect2d rect_roi;
    cv::Mat lineMat;
    QJsonObject resultJson;   //检测结果json

    if (m_run && m_InitOK)
    {
        cv::Mat img_roi;
        rect_roi = getModifiedRect(locate_point);

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
            result = getLine(img_roi, result_data);
        }

        //线Json
        QJsonObject drawLineJson;

        if (result)
        {
            ias::feature::LineTool::getRectCross({{result_data[1] + rect_roi.x, result_data[2] + rect_roi.y},
                                                  {result_data[3] + rect_roi.x, result_data[4] + rect_roi.y}}, lineMat, rect_roi);
            //线Json
            QVector<QJsonObject> pointJsonVector;
            pointJsonVector.append(genPointJson(lineMat.at<cv::Point>(0).x, lineMat.at<cv::Point>(0).y));
            pointJsonVector.append(genPointJson(lineMat.at<cv::Point>(1).x, lineMat.at<cv::Point>(1).y));

            drawLineJson = genDrawObjJson("point", pointJsonVector, DRAW_COLOR_IKB);
        }
        else
        {
            if (result_data[1] == result_data[2] && result_data[2] == result_data[3] && result_data[3] == result_data[4] && result_data[4] == 0)
            {
            }
            else
            {
                ias::feature::LineTool::getRectCross({{result_data[1] + rect_roi.x, result_data[2] + rect_roi.y},
                                                      {result_data[3] + rect_roi.x, result_data[4] + rect_roi.y}}, lineMat, rect_roi);
                //线Json
                QVector<QJsonObject> pointJsonVector;
                pointJsonVector.append(genPointJson(lineMat.at<cv::Point>(0).x, lineMat.at<cv::Point>(0).y));
                pointJsonVector.append(genPointJson(lineMat.at<cv::Point>(1).x, lineMat.at<cv::Point>(1).y));

                drawLineJson = genDrawObjJson("point", pointJsonVector, DRAW_COLOR_RED);
            }
        }

        //矩形Json
        QJsonObject drawRectJson;
        QVector<QJsonObject> pointJsonVector;
        pointJsonVector.append(genPointJson(rect_roi.x, rect_roi.y));
        pointJsonVector.append(genPointJson(rect_roi.br().x, rect_roi.br().y));
        if (result)
        {
            drawRectJson = genDrawObjJson("rect", pointJsonVector, DRAW_COLOR_GREEN);
        }
        else
        {
            drawRectJson = genDrawObjJson("rect", pointJsonVector, DRAW_COLOR_RED);
        }

        resultJson.insert("draw", genDrawJson(drawLineJson, drawRectJson));
        resultJson.insert("line_score", result_data[0]);
    }
    else if (!m_run)
    {
        result = true;
    }

    resultJson.insert("id", getId());
    resultJson.insert("method", DETECT_METHOD_HOUGH_LINE);
    resultJson.insert("dtType",  DT_TYPE_DETECTOR);
    resultJson.insert("result", result ? 1 : 0);

    m_resultJson = resultJson;  //部分分支不对m_resultJson赋值，所以需要有引入中间变量resultJson

    //检测结果
    BaseDTResult detectResult;

    detectResult.setId(getId());
    detectResult.setResult(result);
    detectResult.setPositionInfo(QJsonDocument(m_resultJson).toJson().toStdString());

    return detectResult;
}
