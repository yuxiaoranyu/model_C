#include "PatchCoreInference.h"
#include "../common/IasCommonDefine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>

PatchCoreInferenceFactory PatchCoreFactory;

void PatchCoreInferenceInit()
{
    RegisterBaseAiInferenceFactory(PatchCore_COMPONENT_TYPE, &PatchCoreFactory);
}

PatchCoreInference::PatchCoreInference()
{
}

PatchCoreInference::~PatchCoreInference()
{
    remove();
}

bool PatchCoreInference::remove()
{
    std::map<int, PatchCore *>::iterator it;

    for (it = m_camera_map.begin(); it != m_camera_map.end(); ++it)
    {
        it->second->cleanup();
        delete it->second;
    }
    m_camera_map.clear();

    return true;
}

bool PatchCoreInference::load(const std::string &param)
{
    std::string aiPath = getPath(); //获取算法路径

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();

    QJsonObject camera_param = jsonDocObj.value("camera_param").toObject();
    QJsonObject module_param = jsonDocObj.value("module_param").toObject();
    bool btemplate_match = jsonDocObj.value("global_param").toObject().value("template_matching").toBool();

    QJsonObject moduleJsonObject = module_param.value("moduleId_0").toObject();
    std::string moduleMethod = moduleJsonObject.value("method").toString().toStdString();

    if (moduleMethod == "patchcore")
    {
        for (int camera_id = 1; camera_id <= camera_param.size(); camera_id++)
        {
            QJsonObject cameraJsonObject = camera_param.value(QString("cameraId_%1").arg(camera_id)).toObject();
            Config_Data config;

            config.model_path = aiPath + "/resource/" + std::to_string(camera_id) + ".pt";
            config.template_path = aiPath + "/template/" + std::to_string(camera_id);
            config.btemplate_match = btemplate_match;
            config.heat_threshold = cameraJsonObject.value("heat_threshold").toString().toInt();
            config.heat_binary_min_area = cameraJsonObject.value("heat_binary_min_area").toString().toInt();
            config.defective_min_area = cameraJsonObject.value("defective_min_area").toString().toInt();
            config.expand_pixel = cameraJsonObject.value("expand_pixel").toString().toInt();
            config.diff_threshold = cameraJsonObject.value("diff_threshold").toString().toInt();
            config.dismatched_point_threshold = cameraJsonObject.value("dismatched_point_threshold").toString().toInt();
            config.score_min_threshold = cameraJsonObject.value("score_min_threshold").toString().toFloat();
            config.score_max_threshold = cameraJsonObject.value("score_max_threshold").toString().toFloat();
            QJsonObject effective_area = cameraJsonObject.value("effective_area").toObject();
            QJsonArray points = effective_area.value("points").toArray();
            for (int i = 0; i < points.size(); i++)
            {
                cv::Point point;
                point.x = points[i].toObject().value("x").toInt();
                point.y = points[i].toObject().value("y").toInt();
                config.effective_area.push_back(point);
            }
            PatchCore *patchcore = new PatchCore();
            m_camera_map[camera_id] = patchcore;
            patchcore->init(config);
        }
    }

    return true;
}

bool PatchCoreInference::update(const std::string &param)
{
    (void)param;

    return true;
}

std::vector<BaseAIResult> PatchCoreInference::infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images)
{
    BaseAIResult inferResult;
    std::vector<BaseAIResult> inferResultVector;

    if (ids.size() > 1)
    {
        qInfo() << "PatchCoreInference error, only support one image infer, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }

    if (m_camera_map[ids[0]] == nullptr)
    {
        inferResult.setResult(false);
        inferResultVector.push_back(inferResult);
        return inferResultVector;
    }
    QJsonArray pos_infoArray;
    QJsonArray kickObjArray;
    bool result = true;
    PatchCore_Result patchcore_result = m_camera_map[ids[0]]->infer(images);
    if (!patchcore_result.result)
    {
        result = false;
        QJsonArray pointArray;    //构建图形单元points的json
        QJsonObject ltObj, rbObj; //左上角坐标,右上角坐标
        ltObj.insert("x", patchcore_result.ltx);
        ltObj.insert("y", patchcore_result.lty);
        rbObj.insert("x", patchcore_result.rbx);
        rbObj.insert("y", patchcore_result.rby);
        pointArray.append(ltObj);
        pointArray.append(rbObj);

        QJsonObject drawObj;  //图形单元
        QJsonArray drawArray; //图形单元数组

        drawObj.insert("points", pointArray);
        drawObj.insert("color", "FF0000");
        drawObj.insert("shapeType", 2);
        drawArray.append(drawObj);

        QJsonObject paramObj;    //构建算法返回参数params的json
        QJsonObject pos_infoObj; //构建pos_info的json
        pos_infoObj.insert("id", 0);
        pos_infoObj.insert("moduleId", 1);
        pos_infoObj.insert("draw", drawArray);
        pos_infoObj.insert("result", 0);
        pos_infoObj.insert("params", paramObj);
        pos_infoArray.append(pos_infoObj);
    }

    inferResult.setResult(result);

    QString pos_infoSrting = QJsonDocument(pos_infoArray).toJson();
    inferResult.setPositionInfo(pos_infoSrting.toStdString());
//    std::cout<<"result*********************"<<inferResult.getPositionInfo()<<std::endl;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        inferResultVector.push_back(inferResult);
    }

    return inferResultVector;
}
