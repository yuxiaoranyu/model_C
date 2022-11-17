#include "MixBrandInference.h"
#include "../common/IasCommonDefine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>

MixBrandInferenceFactory MixBrandFactory;

void MixBrandInferenceInit()
{
    RegisterBaseAiInferenceFactory(Mix_Brand_COMPONENT_TYPE, &MixBrandFactory);
}

MixBrandInference::MixBrandInference()
{
}

MixBrandInference::~MixBrandInference()
{
    remove();
}

bool MixBrandInference::remove()
{
    if (yoloparam.yolo_model != nullptr)
    {
        yoloparam.yolo_model->cleanup();
        delete yoloparam.yolo_model;
        yoloparam.yolo_model = nullptr;
    }

    if (resnetparam.resnet_model != nullptr)
    {
        resnetparam.resnet_model->cleanup();
        delete resnetparam.resnet_model;
        resnetparam.resnet_model = nullptr;
    }

    return true;
}

bool MixBrandInference::load(const std::string &param)
{
    std::string aiPath = getPath(); //获取算法路径

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();
    QJsonObject module_param = jsonDocObj.value("module_param").toObject();

    if (module_param.size() != 2)
    {
        qInfo() << "YoloResnetInference load: module_param.size() != 2";
        return false;
    }

    for (int i = 0; i < module_param.size(); i++)
    {
        QString moduleIdString = QString::fromStdString("moduleId_" + std::to_string(i));
        QJsonObject moduleJsonObject = module_param.value(moduleIdString).toObject();
        std::string moduleMethod = moduleJsonObject.value("method").toString().toStdString();
        if (moduleMethod == "yolov5")
        {
            //初始化模型
            yoloparam.model_path = aiPath + "/resource/1.engine";
            yoloparam.yolo_model = new Yolov5();
            yoloparam.yolo_model->init(yoloparam.model_path);
        }
        else
        {
            //初始化模型
            resnetparam.model_path = aiPath + "/resource/2.engine";
            resnetparam.resnet_model = new ResNet();
            resnetparam.resnet_model->init(resnetparam.model_path);

            //加载标签文件
            QFile file(QString::fromStdString(aiPath + "/resource/2.lbl"));
            if (file.open(QIODevice::ReadOnly))
            {
                QByteArray all_data = file.readAll();
                file.close();

                QJsonParseError json_error;
                QJsonDocument jsonDoc(QJsonDocument::fromJson(all_data, &json_error));
                QJsonObject rootObj = jsonDoc.object();
                resnetparam.label_array = rootObj["labels"].toArray();
            }
        }
    }

    return true;
}



bool MixBrandInference::update(const std::string &param)
{
    (void)param;

    return true;
}

std::vector<BaseAIResult> MixBrandInference::infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images)
{
    BaseAIResult inferResult;
    std::vector<BaseAIResult> inferResultVector;

    if (ids.size() > 1) {
        qInfo() << "YoloResnetInference error, only support one image infer, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }

    QJsonArray pos_infoArray;
    QJsonArray kickObjArray;
    bool result = true;

    std::vector<std::vector<YoloResult>> yolo_result_data = yoloparam.yolo_model->infer(images);

    //目前batch=1，所以yolo_result_data.size固定为1
    for (size_t yolo_index = 0; yolo_index < yolo_result_data.size(); yolo_index++)
    {
//        std::cout << "yolo result size " << yolo_result_data[yolo_index].size() << std::endl;
        if (yolo_result_data[yolo_index].size() != 0)  //仅分析检测出的第一个目标
        {
            int x1 = yolo_result_data[yolo_index][0].box.tl().x;
            int y1 = yolo_result_data[yolo_index][0].box.tl().y;
            int x2 = yolo_result_data[yolo_index][0].box.br().x;
            int y2 = yolo_result_data[yolo_index][0].box.br().y;
/*
            std::cout << "******************x1******************** " << x1 << std::endl;
            std::cout << "******************x2******************** " << x1 << std::endl;
            std::cout << "******************y1******************** " << y1 << std::endl;
            std::cout << "******************y2******************** " << y2 << std::endl;
*/
            cv::Rect cropRect(cv::Point(std::max(x1, 0), std::max(y1, 0)), cv::Point(std::min(x2, images[0].cols), std::min(y2, images[0].rows)));
            std::vector<cv::Mat> resnet_input;
            resnet_input.push_back(images[yolo_index](cropRect));
            std::vector<ResNetResult> resnet_result_data = resnetparam.resnet_model->infer(resnet_input);

            int classId = resnet_result_data[yolo_index].classId; //检测目标的分类ID

            QJsonObject labelJsonObj = resnetparam.label_array[classId].toObject();
            bool isDefective = labelJsonObj["isDefective"].toBool();
            QString aliasString = labelJsonObj["alias"].toString();
            QString colorString = labelJsonObj["color"].toString("FF0000");

            if (isDefective) //杂物类别
            {
                result = false;
                std::vector<cv::Point2f> in_pos;  //杂物图像坐标
                std::vector<cv::Point2f> out_pos; //杂物物理坐标

                in_pos.push_back(cv::Point2f(x1 + x2 / 2, y1 + y2 / 2));

                QJsonArray pointArray;  //构建图形单元points的json
                QJsonObject ltObj, rbObj; //左上角坐标,右上角坐标

                ltObj.insert("x", x1);
                ltObj.insert("y", y1);
                rbObj.insert("x", x2);
                rbObj.insert("y", y2);
                pointArray.append(ltObj);
                pointArray.append(rbObj);

                QJsonObject drawObj; //图形单元
                QJsonArray drawArray; //图形单元数组

                drawObj.insert("points", pointArray);
                drawObj.insert("color", colorString);
                drawObj.insert("shapeType", 2);
                drawArray.append(drawObj);

                QJsonObject kickObj;  //烟丝杂物剔除坐标点json
                kickObj.insert("x", (int)out_pos[0].x);
                kickObj.insert("y", (int)out_pos[0].y);

                QJsonObject paramObj; //构建算法返回参数params的json
                paramObj.insert("kick_coordinate", kickObj);

                QJsonObject pos_infoObj;   //构建pos_info的json
                pos_infoObj.insert("id", int(yolo_index));
                pos_infoObj.insert("moduleId", 1);
                pos_infoObj.insert("classes", classId);
                pos_infoObj.insert("label", aliasString);
                pos_infoObj.insert("draw", drawArray);
                pos_infoObj.insert("result", 0);
                pos_infoObj.insert("params", paramObj);
                pos_infoArray.append(pos_infoObj);

                kickObjArray.append(kickObj);
            }
        }
    }
    inferResult.setResult(result);

    QString pos_infoSrting = QJsonDocument(pos_infoArray).toJson();
    inferResult.setPositionInfo(pos_infoSrting.toStdString());

    if (kickObjArray.size() > 0) //构建IsgInfo
    {
        QJsonObject isgObj;

        isgObj.insert("kick_coordinate", kickObjArray);
        QString isgInfoString = QJsonDocument(isgObj).toJson();
        inferResult.setIsgInfo(isgInfoString.toStdString());
    }

    for (size_t i = 0; i < ids.size(); ++i)
    {
        inferResultVector.push_back(inferResult);
    }
//    std::cout << "infer result ***************************** " << inferResult.getPositionInfo() << std::endl;
    return inferResultVector;
}
