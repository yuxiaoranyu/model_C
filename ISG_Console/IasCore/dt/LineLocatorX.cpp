#include "LineLocatorX.h"
#include "DtCommon.h"
#include "../common/GenJsonObject.h"
#include "IasCore/dt/api/lines.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

LineLocatorXFactory lineLocatorXFactory;

void LineLocatorXInit()
{
    RegisterBaseLocatorFactory(LOCATE_METHOD_HOUGH_LINE_X, &lineLocatorXFactory);
}

bool LineLocatorX::load(const std::string &param)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    m_config = param;
    m_InitOK = false;

    m_run = jsonDocObj.value("run").toBool(true);
    locate_direction = jsonDocObj.value("locate_direction").toDouble();
    line_length_range = jsonDocObj.value("line_length_range").toDouble();
    canny_threshold = jsonDocObj.value("canny_threshold").toDouble();
    line_angle_begin = jsonDocObj.value("line_angle_begin").toDouble();
    line_angle_end = jsonDocObj.value("line_angle_end").toDouble();
    line_score_threshold = jsonDocObj.value("line_score").toDouble();

    if ((line_length_range < 0) || (line_length_range > 1))
    {
        qInfo() << "LineLocatorX: line_length_range is out of range";
        return false;
    }

    if (!((line_angle_begin >= 0) && (line_angle_begin < line_angle_end) && (line_angle_end <= 180)))
    {
        qInfo() << "LineLocatorX: line_angle is out of range";
        return false;
    }

    if ((canny_threshold < 0) || (canny_threshold > 256))
    {
        qInfo() << "LineLocatorX: canny_threshold is out of range";
        return false;
    }

    QJsonArray pointsArray = jsonDocObj.value("roi").toArray(); //检测区域
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
        qInfo() << "LineLocatorX: roi is not config!";
        return false;
    }

    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg");
    if (matRead.empty())
    {
        qInfo() << "LineLocatorX: can not read channel.jpg";
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

std::string  LineLocatorX::computeParam()
{
    return m_config;
}

BaseDTResult LineLocatorX::locate(const cv::Mat &mat)
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
    m_resultJson.insert("method", LOCATE_METHOD_HOUGH_LINE_X);
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
bool LineLocatorX::getLocateLinePoint(const cv::Mat &frame, double &score, cv::Point &locatePoint)
{
    std::vector<double> locate_line_x;
    ias::feature::Line line(1, line_score_threshold, ias::feature::_SCORE,
                            ias::feature::LINE_METHOD_1,
                            {line_angle_begin, line_angle_end},
                            cvRound(frame.rows * line_length_range));

    std::vector<ias::feature::Line::LinesData> find_lines_data;
    line.setScoringHitKernel_((cv::Mat_<int>(7, 3) <<  0, 1, 0,
                                                       0, 1, 0,
                                                       0, 1, 0,
                                                       0, 1, 0,
                                                       0, 1, 0,
                                                       0, 1, 0,
                                                       0, 1, 0));
    line.setNormalization_(true);
    cv::Mat frame_canny, frame_filter;
    cv::bilateralFilter(frame, frame_filter, 5, 50, 50);
    Canny(frame_filter, frame_canny, canny_threshold, 100, 3);
    line.getLinesMethod1(frame_canny);
    find_lines_data = line.LineNms(frame_canny);
    if (find_lines_data.size() > 0)
    {
        for (auto i : find_lines_data)
        {
            locate_line_x.push_back(line.getPolarSystemPoint(i, "y", frame.rows / 2));
        }
        ias::Sort sort;
        std::vector<size_t> idx = sort.vectorSort(locate_line_x, "UP");
        if (locate_direction == 0)
        {
            //选择最左侧的线作为定位线
            locatePoint = {cvRound(locate_line_x[idx.front()]), cvRound(frame.rows / 2)};
            score = find_lines_data[idx.front()].score;
        }
        else if (locate_direction == 1)
        {
            //选择最右侧的线作为定位线
            locatePoint = {cvRound(locate_line_x[idx.back()]), cvRound(frame.rows / 2)};
            score = find_lines_data[idx.back()].score;
        }
        if (score >= line_score_threshold)
        {
            return true;
        }
    }

    return false;
}
