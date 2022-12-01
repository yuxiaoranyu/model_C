#include "ColorDetector.h"
#include "DtCommon.h"
#include "../common/GenJsonObject.h"
#include "IasCore/dt/api/lines.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

ColorDetectorFactory colorDetectorFactory;

void ColorDetectorInit()
{
    RegisterBaseDetectorFactory(DETECT_METHOD_COLOR, &colorDetectorFactory);
}

//计算均值和标准偏差的差异，是否在设置范围内
bool ColorDetector::ContrastMeanStdDev(const cv::Mat &run_mean, const cv::Mat &this_mean,
                        const cv::Mat &run_stddev, const cv::Mat &this_stddev,
                        const cv::Mat &mean_range, const cv::Mat &stddev_range)
{
    cv::Mat diff_mean = abs(run_mean - this_mean);
    cv::Mat diff_stddev = abs(run_stddev - this_stddev);
    cv::Mat result_mean, result_stddev;

    cv::compare(diff_mean, mean_range, result_mean, cv::CMP_GT);
    cv::compare(diff_stddev, stddev_range, result_stddev, cv::CMP_GT);
    if (countNonZero(result_mean) == 0 && countNonZero(result_stddev) == 0)
    {
        return true;
    }

    return false;
}

bool ColorDetector::load(const std::string &param, const cv::Point2d& locate_point)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    m_config = param;
    m_InitOK = false;

    m_run = jsonDocObj.value("run").toBool(true);

    m_locate_x_id = jsonDocObj.value("locator_x_id").toInt(0);
    m_locate_y_id = jsonDocObj.value("locator_y_id").toInt(0);

    setLocateRoot(locate_point);

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
        qInfo() << "ColorDetector: roi is error";
        return false;
    }

    mean_gray = jsonDocObj.value("mean_gray").toDouble();
    mean_b = jsonDocObj.value("mean_b").toDouble();
    mean_g = jsonDocObj.value("mean_g").toDouble();
    mean_r = jsonDocObj.value("mean_r").toDouble();

    stddev_gray = jsonDocObj.value("stddev_gray").toDouble();
    stddev_b = jsonDocObj.value("stddev_b").toDouble();
    stddev_g = jsonDocObj.value("stddev_g").toDouble();
    stddev_r = jsonDocObj.value("stddev_r").toDouble();

    mean_gray_range = jsonDocObj.value("mean_gray_range").toDouble();
    mean_b_range = jsonDocObj.value("mean_b_range").toDouble();
    mean_g_range = jsonDocObj.value("mean_g_range").toDouble();
    mean_r_range = jsonDocObj.value("mean_r_range").toDouble();

    stddev_gray_range = jsonDocObj.value("stddev_gray_range").toDouble();
    stddev_b_range = jsonDocObj.value("stddev_b_range").toDouble();
    stddev_g_range = jsonDocObj.value("stddev_g_range").toDouble();
    stddev_r_range = jsonDocObj.value("stddev_r_range").toDouble();
    on_gray = jsonDocObj.value("gray").toBool();

    if ((mean_gray < 0) || (mean_r < 0) || (mean_g < 0) || (mean_b < 0) ||
        (stddev_gray < 0) || (stddev_r < 0) || (stddev_g < 0) || (stddev_b < 0) ||
        (mean_gray_range < 0) || (mean_r_range < 0) || (mean_g_range < 0) || (mean_b_range < 0) ||
        (stddev_gray_range < 0) || (stddev_r_range < 0) || (stddev_g_range < 0) || (stddev_b_range < 0))
    {
        qInfo() << "ColorDetector: config json is out of range";
        return false;
    }

    m_InitOK = true;

    return true;
}

std::string ColorDetector::computeParam()
{
    cv::Mat matRead = cv::imread(getPath() + "/" + std::to_string(getGroup())  + "/channel.jpg");
    if (!matRead.empty())
    {
        cv::Mat matRoi = matRead(m_rect_roi);
        if (!matRoi.empty())
        {
            cv::Mat grayRoi;
            cv::Mat gray_mean, gray_stddev, bgr_mean, bgr_stddev;

            cv::cvtColor(matRoi, grayRoi, cv::COLOR_BGR2GRAY);

            cv::meanStdDev(grayRoi, gray_mean, gray_stddev);
            cv::meanStdDev(matRoi, bgr_mean, bgr_stddev);

            mean_gray = gray_mean.at<double>(0, 0);
            mean_b = bgr_mean.at<double>(0, 0);
            mean_g = bgr_mean.at<double>(0, 1);
            mean_r = bgr_mean.at<double>(0, 2);

            stddev_gray = gray_stddev.at<double>(0, 0);
            stddev_b = bgr_stddev.at<double>(0, 0);
            stddev_g = bgr_stddev.at<double>(0, 1);
            stddev_r = bgr_stddev.at<double>(0, 2);

            mean_gray_range = gray_mean.at<double>(0, 0) * 0.25;
            mean_b_range = bgr_mean.at<double>(0, 0) * 0.25;
            mean_g_range = bgr_mean.at<double>(0, 1) * 0.25;
            mean_r_range = bgr_mean.at<double>(0, 2) * 0.25;

            stddev_gray_range = gray_stddev.at<double>(0, 0) * 0.25;
            stddev_b_range = bgr_stddev.at<double>(0, 0) * 0.25;
            stddev_g_range = bgr_stddev.at<double>(0, 1) * 0.25;
            stddev_r_range = bgr_stddev.at<double>(0, 2) * 0.25;

            QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(m_config).toLocal8Bit());
            QJsonObject jsonDocObj = jsonDoc.object();

            jsonDocObj.insert("mean_gray", mean_gray);
            jsonDocObj.insert("mean_b", mean_b);
            jsonDocObj.insert("mean_g", mean_g);
            jsonDocObj.insert("mean_r", mean_r);

            jsonDocObj.insert("stddev_gray", stddev_gray);
            jsonDocObj.insert("stddev_b", stddev_b);
            jsonDocObj.insert("stddev_g", stddev_g);
            jsonDocObj.insert("stddev_r", stddev_r);

            jsonDocObj.insert("mean_gray_range", mean_gray_range);
            jsonDocObj.insert("mean_b_range", mean_b_range);
            jsonDocObj.insert("mean_g_range", mean_g_range);
            jsonDocObj.insert("mean_r_range", mean_r_range);

            jsonDocObj.insert("stddev_gray_range", stddev_gray_range);
            jsonDocObj.insert("stddev_b_range", stddev_b_range);
            jsonDocObj.insert("stddev_g_range", stddev_g_range);
            jsonDocObj.insert("stddev_r_range", stddev_r_range);

            m_config = QString(QJsonDocument(jsonDocObj).toJson()).toStdString();
        }
    }

    return m_config;
}

BaseDTResult ColorDetector::detect(const cv::Mat &frame, const cv::Point2d& locate_point)
{
    cv::Rect2d rect_roi;
    QJsonObject resultJson;    //结果json
    bool result = false;

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

        if ((on_gray) && (img_roi.channels() > 1))
        {
            cv::cvtColor(img_roi, img_roi, cv::COLOR_BGR2GRAY);
        }

        if (!img_roi.empty())
        {
            cv::Mat mean, stddev;

            cv::meanStdDev(img_roi, mean, stddev); //计算矩阵的均值和标准偏差

            switch (mean.rows)
            {
                case 1:  //单色图
                    result = ContrastMeanStdDev(mean, (cv::Mat_<double>(1, 1) << mean_gray),
                                       stddev, (cv::Mat_<double>(1, 1) << stddev_gray),
                                       (cv::Mat_<double>(1, 1) << mean_gray_range),
                                       (cv::Mat_<double>(1, 1) << stddev_gray_range));

                    resultJson.insert("mean_gray", mean.at<double>(0, 0));
                    resultJson.insert("stddev_gray", stddev.at<double>(0, 0));
                    break;

                case 3:  //三色图
                    result = ContrastMeanStdDev(mean, (cv::Mat_<double>(3, 1) << mean_b, mean_g, mean_r),
                                       stddev, (cv::Mat_<double>(3, 1) << stddev_b, stddev_g, stddev_r),
                                       (cv::Mat_<double>(3, 1) << mean_b_range, mean_g_range, mean_r_range),
                                       (cv::Mat_<double>(3, 1) << stddev_b_range, stddev_g_range, stddev_r_range));

                    resultJson.insert("mean_r", mean.at<double>(0, 0));
                    resultJson.insert("mean_g", mean.at<double>(0, 1));
                    resultJson.insert("mean_b", mean.at<double>(0, 2));
                    resultJson.insert("stddev_r", stddev.at<double>(0, 0));
                    resultJson.insert("stddev_g", stddev.at<double>(0, 1));
                    resultJson.insert("stddev_b", stddev.at<double>(0, 2));
                    break;

                default:
                    break;
            }

            QJsonObject drawRectJson;  //多边形Json
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
            resultJson.insert("draw", drawRectJson);
        }
    }
    else if (!m_run)
    {
        result = true;
    }

    resultJson.insert("id", getId());
    resultJson.insert("method", DETECT_METHOD_COLOR);
    resultJson.insert("dtType", DT_TYPE_DETECTOR);
    resultJson.insert("result", result ? 1 : 0);

    m_resultJson = resultJson;  //部分分支不对m_resultJson赋值，所以需要有引入中间变量resultJson

    BaseDTResult detectResult;
    detectResult.setId(getId());
    detectResult.setResult(result);
    detectResult.setPositionInfo(QJsonDocument(m_resultJson).toJson().toStdString());

    return detectResult;
}
