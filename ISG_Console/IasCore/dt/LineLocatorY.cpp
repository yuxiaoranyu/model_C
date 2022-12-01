#include "LineLocatorY.h"
#include "DtCommon.h"
#include "../common/GenJsonObject.h"
#include "IasCore/dt/api/lines.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

LineLocatorYFactory lineLocatorYFactory;

void LineLocatorYInit()
{
    RegisterBaseLocatorFactory(LOCATE_METHOD_HOUGH_LINE_Y, &lineLocatorYFactory);
}

bool LineLocatorY::load(const std::string &param)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    m_config = param;
    m_InitOK = false;

    m_run = jsonDocObj.value("run").toBool(true);
    line_length_range = jsonDocObj.value("line_length_range").toDouble();
    binary_threshold = jsonDocObj.value("binary_threshold").toDouble();
    line_score_threshold = jsonDocObj.value("line_score_threshold").toDouble();
    locate_direction = jsonDocObj.value("locate_direction").toDouble();

    if ((line_length_range < 0) || (line_length_range > 1))
    {
        qInfo() << "LineLocatorY: line_length_range is out of range";
        return false;
    }
    if ((binary_threshold < 0) || (binary_threshold > 256))
    {
        qInfo() << "LineLocatorY: binary_threshold is out of range";
        return false;
    }

    QJsonArray pointsArray = jsonDocObj.value("roi").toArray();  //加载检测区域
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
        qInfo() << "LineLocatorY: roi is error";
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
    line_angle_range[0] = ias::feature::LineTool::getLineAngle(line_point[1], line_point[0]);
    line_angle_range[1] = jsonDocObj.value("angle_range").toDouble();
    locate_direction = 1;

    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg");
    if (matRead.empty())
    {
        return false;
    }
    else
    {
        cv::Mat roiMat = (matRead)(m_rect_roi);

        double score;
        cv::Point localPoint = {0, 0};

        bool bResult = getLocateLinePoint(roiMat, score, localPoint);  //获取静态图片定位点
        if (bResult)
        {
            localPoint.x += m_rect_roi.tl().x;
            localPoint.y += m_rect_roi.tl().y;
        }
        setLocateRoot(localPoint);
        m_InitOK = true;
        return true;
    }
}

std::string  LineLocatorY::computeParam()
{
    return m_config;
}

BaseDTResult LineLocatorY::locate(const cv::Mat &mat)
{
    bool result = false;   //定位结果
    cv::Point localPoint = {0, 0};  //定位点
    double score = 0; //得分

    if (m_run && m_InitOK)
    {
        cv::Mat roiMat = (mat)(m_rect_roi);
        if (!roiMat.empty())
        {
            result = getLocateLinePoint(roiMat, score, localPoint);
            if (result)
            {
                localPoint.x += m_rect_roi.tl().x;
                localPoint.y += m_rect_roi.tl().y;
            }
        }
    }
    else if (!m_run)
    {
        result = true;
        localPoint.x += m_rect_roi.tl().x;
        localPoint.y += m_rect_roi.tl().y;
        score = 100;
    }

    QJsonObject drawPointJson; //点Json
    if (result)
    {
        QVector<QJsonObject> pointJsonVector;
        pointJsonVector.append(genPointJson(localPoint.x, localPoint.y));
        drawPointJson = genDrawObjJson("point", pointJsonVector, DRAW_COLOR_IKB);
    }
    //矩形Json
    QVector<QJsonObject> rectPointJsonVector;
    rectPointJsonVector.append(genPointJson(m_rect_roi.x, m_rect_roi.y));
    rectPointJsonVector.append(genPointJson(m_rect_roi.br().x, m_rect_roi.br().y));
    QJsonObject drawRectJson = genDrawObjJson("rect", rectPointJsonVector, result ? DRAW_COLOR_GREEN : DRAW_COLOR_RED);

    m_resultJson.insert("id", getId());
    m_resultJson.insert("method", LOCATE_METHOD_HOUGH_LINE_Y);
    m_resultJson.insert("dtType", DT_TYPE_LOCATOR);
    m_resultJson.insert("result", result ? 1 : 0);
    m_resultJson.insert("line_score", score);
    m_resultJson.insert("locate_point", genPointJson(localPoint.x, localPoint.y));
    m_resultJson.insert("draw", genDrawJson(drawPointJson, drawRectJson));

    //检测结果
    BaseDTResult locateResult;

    locateResult.setId(getId());
    locateResult.setResult(result);
    locateResult.setLocatePoint(localPoint);
    locateResult.setPositionInfo(QJsonDocument(m_resultJson).toJson().toStdString());

    return locateResult;
}

//用Hough查找定位线，线取中点做定位点
bool LineLocatorY::getLocateLinePoint(const cv::Mat &frame, double &score, cv::Point &locatePoint)
{
    std::vector<double> locate_line_y;
    cv::Mat frame_line;

    ias::feature::Line line(10, 0, ias::feature::HOUGH_SCORE,
                            ias::feature::LINE_METHOD_2,
                            line_angle_range,
                            line_length_range * line_length);

    Canny(frame, frame_line, binary_threshold, 100, 3);  //边缘查找
    line.getLinesMethod2(frame_line);
    auto find_lines_data = line.LineNms(frame);
    if (find_lines_data.size() > 0)
    {
        for (auto i : find_lines_data)
        {
            locate_line_y.push_back(line.getPolarSystemPoint(i, "x", frame.cols / 2));
        }
        ias::Sort sort;
        std::vector<size_t> idx = sort.vectorSort(locate_line_y, "UP");
        if (locate_direction == 0)
        {
            //选择最上侧的线作为定位线
            locatePoint = {cvRound(frame.cols / 2), cvRound(locate_line_y[idx.front()])};
            score = find_lines_data[idx.front()].score;
        }
        else if (locate_direction == 1)
        {
            //选择最下侧的线作为定位线
            locatePoint = {cvRound(frame.cols / 2), cvRound(locate_line_y[idx.back()])};
            score = find_lines_data[idx.back()].score;
        }
    }

    if (score >= line_score_threshold)
    {
        return true;
    }

    return false;
}
