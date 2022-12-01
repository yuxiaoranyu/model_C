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
        qInfo() << "MixBrandInference load: module_param.size() != 2";
        return false;
    }

    for (int i = 0; i < module_param.size(); i++)
    {
        QString moduleIdString = QString::fromStdString("moduleId_" + std::to_string(i));
        QJsonObject moduleJsonObject = module_param.value(moduleIdString).toObject();
        std::string moduleMethod = moduleJsonObject.value("method").toString().toStdString();
        if ((moduleMethod == "yolov5") && (i == 0))
        {
            //初始化模型
            yoloparam.yolo_model = new Yolov5();
            if (!yoloparam.yolo_model->init(aiPath + "/resource/0.engine"))
            {
                qInfo()<<"MixBrand yolov5 init failed!";
                return false;
            }
        }
        else if ((moduleMethod == "resnet") && (i == 1))
        {
            //初始化模型
            resnetparam.resnet_model = new ResNet();
            if (!resnetparam.resnet_model->init(aiPath + "/resource/1.engine"))
            {
                qInfo()<<"MixBrand resnet init failed!";
                return false;
            }

            //加载标签文件
            QFile file(QString::fromStdString(aiPath + "/resource/1.lbl"));
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
        else
        {
            return false;
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

    if (ids.size() > 1)
    {
        qInfo() << "MixBrandInference error, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }

    QJsonArray pos_infoArray;
    bool result = true;

    std::vector<std::vector<YoloResult>> yoloResult;

    if (yoloparam.yolo_model->infer(images, yoloResult))
    {
        //目前batch=1，所以yolo_result_data.size固定为1
        for (size_t yolo_index = 0; yolo_index < yoloResult.size(); yolo_index++)
        {
            QJsonObject yolo_posinfoObj, yolo_paramsObj; //构建pos_info的json
            QJsonArray yolo_drawArray;                   //图形单元数组

            for (size_t result_index = 0; result_index < yoloResult[yolo_index].size(); result_index++)
            {
                std::vector<cv::Mat> resnet_input;
                int x1 = yoloResult[yolo_index][result_index].box.tl().x;
                int y1 = yoloResult[yolo_index][result_index].box.tl().y;
                int x2 = yoloResult[yolo_index][result_index].box.br().x;
                int y2 = yoloResult[yolo_index][result_index].box.br().y;
                QJsonArray yolo_pointArray;    //构建图形单元points的json
                QJsonObject yolo_ltObj, yolo_rbObj,yolo_drawObj; //左上角坐标,右上角坐标
                yolo_ltObj.insert("x",x1);
                yolo_ltObj.insert("y",y1);
                yolo_rbObj.insert("x",x2);
                yolo_rbObj.insert("y",y2);
                yolo_pointArray.append(yolo_ltObj);
                yolo_pointArray.append(yolo_rbObj);
                yolo_drawObj.insert("color", "08A7FA");
                yolo_drawObj.insert("points",yolo_pointArray);
                yolo_drawObj.insert("shapeType", 2);
                yolo_drawArray.append(yolo_drawObj);
            }
            yolo_posinfoObj.insert("id", 0);
            yolo_posinfoObj.insert("moduleId", 0);  //yolo结果
            yolo_posinfoObj.insert("classes", "null");
            yolo_posinfoObj.insert("label", "it is a label");
            yolo_posinfoObj.insert("draw", yolo_drawArray);
            yolo_paramsObj.insert("target_num", int(yoloResult[yolo_index].size()));
            yolo_posinfoObj.insert("params", yolo_paramsObj);
            if (yoloResult[yolo_index].size() == 25)  //烟箱堆垛的烟条数目为25
            {
                yolo_posinfoObj.insert("result", 1);
                result = true;
            }
            else {
                yolo_posinfoObj.insert("result", 0);
                result = false;
            }
            pos_infoArray.append(yolo_posinfoObj);

            if (result)
            {
                for (size_t result_index = 0; result_index < yoloResult[yolo_index].size(); result_index++)
                {
                    std::vector<cv::Mat> resnetInput;
                    int x1 = yoloResult[yolo_index][result_index].box.tl().x;
                    int y1 = yoloResult[yolo_index][result_index].box.tl().y;
                    int x2 = yoloResult[yolo_index][result_index].box.br().x;
                    int y2 = yoloResult[yolo_index][result_index].box.br().y;
                    cv::Rect crop(cv::Point(std::max(x1, 0), std::max(y1, 0)),
                                  cv::Point(std::min(x2, images[0].cols), std::min(y2, images[0].rows)));
                    resnetInput.push_back(images[0](crop));
                    std::vector<ResNetResult> resnetResult;
                    if (resnetparam.resnet_model->infer(resnetInput, resnetResult))
                    {

                        int classId = resnetResult[result_index].classId;
                        QJsonObject labelJsonObj = resnetparam.label_array[classId].toObject();
                        QString aliasString = labelJsonObj["alias"].toString();
                        QString colorString = labelJsonObj["color"].toString("FF0000");
                        bool isDefective = labelJsonObj["isDefective"].toBool();
                        if (isDefective)
                        {
                            QJsonArray pointArray;    //构建图形单元points的json
                            QJsonObject ltObj, rbObj; //左上角坐标,右上角坐标

                            ltObj.insert("x", x1);
                            ltObj.insert("y", y1);
                            rbObj.insert("x", x2);
                            rbObj.insert("y", y2);
                            pointArray.append(ltObj);
                            pointArray.append(rbObj);
                            QJsonObject resnet_drawObj, resnet_posinfoObj, resnet_paramsObj; //图形单元
                            QJsonArray resnet_drawArray;                                     //图形单元数组

                            resnet_drawObj.insert("points", pointArray);
                            resnet_drawObj.insert("color", colorString);
                            resnet_drawObj.insert("shapeType", 2);
                            resnet_drawArray.append(resnet_drawObj);
                            resnet_posinfoObj.insert("id", int(result_index + 1));
                            resnet_posinfoObj.insert("moduleId", 1);
                            resnet_posinfoObj.insert("classes", classId);
                            resnet_posinfoObj.insert("label", aliasString);
                            resnet_posinfoObj.insert("draw", resnet_drawArray);
                            resnet_posinfoObj.insert("result", 0);
                            resnet_posinfoObj.insert("params", resnet_paramsObj);
                            pos_infoArray.append(resnet_posinfoObj);
                            result = false;
                        }
                    }
                }
            }
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
