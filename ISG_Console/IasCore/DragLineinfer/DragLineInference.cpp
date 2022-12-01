#include "DragLineInference.h"
#include "../common/IasCommonDefine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <QString>

DragLineInferenceFactory DragLineInferenceFactory;

void DragLineInferenceInit()
{
    RegisterBaseAiInferenceFactory(DragLine_COMPONENT_TYPE, &DragLineInferenceFactory);
}

DragLineInference::DragLineInference()
{
}

DragLineInference::~DragLineInference()
{
    remove();
}

bool DragLineInference::remove()
{
    if (yoloparam.yolo_model != nullptr)
    {
        yoloparam.yolo_model->cleanup();
        delete yoloparam.yolo_model;
        yoloparam.yolo_model = nullptr;
    }

    return true;
}

bool DragLineInference::load(const std::string &param)
{
    std::string aiPath = getPath(); //获取算法路径

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();
    QJsonObject module_param = jsonDocObj.value("module_param").toObject();
    Camera_Params = jsonDocObj.value("camera_param").toObject();
    int gpu_type = jsonDocObj.value("global_param").toObject().value("gpu_type").toInt();

    for (int i = 0; i < Camera_Params.keys().size(); i++){
        int camera_id = Camera_Params.keys()[i].split("_")[1].toInt();
        QJsonObject detect_Area = Camera_Params.value(QString("cameraId_%1").arg(camera_id)).toObject().value("detect_area").toObject();
        detect_area[camera_id].x = detect_Area.value("points").toArray()[0].toObject().value("x").toInt();
        detect_area[camera_id].y = detect_Area.value("points").toArray()[0].toObject().value("y").toInt();
        detect_area[camera_id].width = detect_Area.value("points").toArray()[1].toObject().value("x").toInt() - detect_area[camera_id].x;
        detect_area[camera_id].height = detect_Area.value("points").toArray()[1].toObject().value("y").toInt() - detect_area[camera_id].y;
    }

    QJsonObject moduleJsonObject = module_param.value("moduleId_0").toObject();
    std::string moduleMethod = moduleJsonObject.value("method").toString().toStdString();

    if (moduleMethod == "yolov5")
    {
        //初始化模型
        yoloparam.yolo_model = new Yolov5();
        if (yoloparam.yolo_model->init(aiPath + "/resource/0.engine", gpu_type))
        {
            return true;
        }
    }

    qInfo() << "DragLineInference load failed!";
    return false;
}

bool DragLineInference::update(const std::string &param)
{
    (void)param;

    return true;
}

std::vector<BaseAIResult> DragLineInference::infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images)
{
    BaseAIResult inferResult;
    std::vector<BaseAIResult> inferResultVector;
    QJsonArray pos_infoArray;
    if (ids.size() > 1)
    {
        qInfo() << "DragLineInference error, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }
    int ltx = detect_area[ids[0]].tl().x;
    int lty = detect_area[ids[0]].tl().y;
    int rbx = detect_area[ids[0]].br().x;
    int rby = detect_area[ids[0]].br().y;

    //检测区域放大后的坐标
    int detect_ltx = ltx - 20;
    int detect_lty = lty - 20;
    int detect_rbx = rbx + 20;
    int detect_rby = rby + 20;
    cv::Rect detect_area(cv::Point(detect_ltx, detect_lty), cv::Point(detect_rbx, detect_rby));
    std::vector<cv::Mat> input_images;
    input_images.push_back(images[0](detect_area));
    std::vector<std::vector<YoloResult>> yoloResult;

    bool result = true;
    bool yoloInferResult;

    if (detect_area.area() == 0 || detect_area.area() == 1)
    {
        yoloInferResult = yoloparam.yolo_model->infer(images, yoloResult);
    }
    else
    {
        yoloInferResult = yoloparam.yolo_model->infer(input_images, yoloResult);
    }

    if (yoloInferResult)
    {
        //目前batch=1，所以yolo_result_data.size固定为1
        std::vector<cv::Rect> box;
        std::vector<float> conf;
        for (size_t i = 0; i < yoloResult.size(); i++)
        {
            for (size_t j = 0; j < yoloResult[i].size(); j++)
            {
                if (yoloResult[i][j].classId == 0)
                {
                    box.push_back(yoloResult[i][j].box);
                    conf.push_back(yoloResult[i][j].conf);
                }
            }
        }

        QJsonObject paramsObj;
        if (box.size() == 2)
        {
            //错牙的计算，是以box的X中心坐标进行的
            int diff = abs((box[0].tl().x + box[0].br().x) / 2 - (box[1].tl().x + box[1].br().x) / 2);
            int diff_threshold = Camera_Params.value(QString("cameraId_%1").arg(ids[0])).toObject().value("diff_threshold").toInt();

            paramsObj.insert("diff", diff);
            result = (diff < diff_threshold) ? true : false;
        }
        //检测出0/1个提耳
        else
        {
            result = false;
        }

        for (size_t box_index = 0; box_index < box.size(); box_index++)
        {
            QJsonObject pos_infoObj, drawObj, ltObj, rbObj;
            QJsonArray draw_Array, points_Array;
            int x1, x2, y1, y2;
            if (detect_area.area() != 0)
            {
                x1 = box[box_index].tl().x + detect_ltx;
                y1 = box[box_index].tl().y + detect_lty;
                x2 = box[box_index].br().x + detect_ltx;
                y2 = box[box_index].br().y + detect_lty;
            }
            else
            {
                x1 = box[box_index].tl().x;
                y1 = box[box_index].tl().y;
                x2 = box[box_index].br().x;
                y2 = box[box_index].br().y;
            }
            if ((x1 < ltx) || (y1 < lty) || (x2 > rbx) || (y2 > rby))
            {
                paramsObj.insert("in_area", false);
                drawObj.insert("color", "FF0000");
                pos_infoObj.insert("result", 0);
                result = false;
            }
            else
            {
                paramsObj.insert("in_area", true);
                drawObj.insert("color", "08A7FA");
                pos_infoObj.insert("result", 1);
            }

            drawObj.insert("shape_type", 2);
            ltObj.insert("x", x1);
            ltObj.insert("y", y1);
            rbObj.insert("x", x2);
            rbObj.insert("y", y2);
            points_Array.append(ltObj);
            points_Array.append(rbObj);
            drawObj.insert("points", points_Array);
            draw_Array.append(drawObj);

            paramsObj.insert("conf", conf[box_index]);

            pos_infoObj.insert("id", int(box_index)+1);
            pos_infoObj.insert("moduleId", 1);
            pos_infoObj.insert("classes", 0);
            pos_infoObj.insert("label", "drag_line");
            pos_infoObj.insert("draw", draw_Array);
            pos_infoObj.insert("params", paramsObj);
            pos_infoArray.append(pos_infoObj);
        }
    }

    inferResult.setResult(result);

    QString pos_infoSrting = QJsonDocument(pos_infoArray).toJson();
    inferResult.setPositionInfo(pos_infoSrting.toStdString());

    for (size_t i = 0; i < ids.size(); ++i)
    {
        inferResultVector.push_back(inferResult);
    }
    return inferResultVector;
}
