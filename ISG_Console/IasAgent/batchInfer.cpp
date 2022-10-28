#include "batchInfer.h"
#include "../BaseLib/EngineProject.h"
#include "../ImgSrc/ImageSrcDev.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

QHash<int, BaseAiInferenceFactory *> baseAiInferenceFactoryHash;

BatchInfer::BatchInfer(int id, EngineProject *project, AICtrl *pAICtrl) :
    m_project(project),
    m_AICtrl(pAICtrl),
    m_aiInference(nullptr),
    m_algorithmJsonParser(nullptr),
    m_channelID(id),
    m_bInitOK(false),
    m_jsonChecksum(0)
{
    m_algorithmJsonFileName= "AlgoConfig.json";
}

bool BatchInfer::init()
{
    //向ImageGroup订阅图像组信号，接收成组的图像
    connect(m_AICtrl->getImageGroup(), SIGNAL(imageGroupSignal(const IMAGE_GROUP_DATA &)), this, SLOT(onImageGroup(const IMAGE_GROUP_DATA &)));

    moveToThread(&m_thread);
    m_thread.setObjectName(QString("BatchInfer_%1").arg(m_channelID));
    m_thread.start();
    return true;
}

//执行文件的路径/algorithm/牌号/xx/AlgoConfig.json
bool BatchInfer::loadAlgorithmConfig(const QString &algorithmJsonFilePath)
{
    m_algorithmJsonFilePath = algorithmJsonFilePath + QString("/%1").arg(m_channelID);
    m_jsonChecksum = getAlgorithmFileChecksum();

    m_algorithmJsonParser = new IasJsonParser(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);

    QJsonArray jsonArray = m_algorithmJsonParser->get("channel/algorithms").toArray();

    if (!jsonArray.isEmpty()) {
        QJsonObject jsonObj = jsonArray.at(0).toObject();
        bool bBatch = jsonObj.value("batch").toBool(false);
        m_AICtrl->getImageGroup()->setBatch(bBatch);
        QJsonArray cameraIdArray = jsonObj.value("cameraId").toArray();
        for (int i = 0; i < cameraIdArray.count(); ++i)
        {
            QJsonValue cameraIdValue = cameraIdArray.at(i);

            int cameraId = cameraIdValue.toInt(-1);
            if (cameraId > -1)
            {
                addCameraId(cameraId);
                m_AICtrl->getImageGroup()->addCameraId(cameraId);
            }
        }

        QMetaObject::invokeMethod(this, "initModel");  //启动阶段，在主线程中被调用
    }

    return true;
}

bool BatchInfer::initModel()
{
    qDebug().noquote() << "init ai model: " << m_channelID;

    QJsonArray jsonArray = m_algorithmJsonParser->get("channel/algorithms").toArray();

    if (!jsonArray.isEmpty()) {
        addAiInference(jsonArray.at(0).toObject());
        return true;
    }
    return false;
}

void BatchInfer::cleanup()
{
    m_thread.quit();
    m_thread.wait();

    releaseModel();
}

void BatchInfer::releaseModel()
{
    if (!m_bInitOK) {
        return;
    }
    if (m_aiInference != nullptr) {
        m_aiInference->remove();
        delete m_aiInference;
        m_aiInference = nullptr;
    }
    m_bInitOK = false;
}

void BatchInfer::onImageGroup(const IMAGE_GROUP_DATA &imageGroup)
{
    std::vector<int> cameraIdVector;
    std::vector<cv::Mat> imageDataVector;
    std::vector<BaseAIResult> resultVector;

    if (!m_bInitOK) {
        for (size_t i = 0; i < (size_t)imageGroup.cameraIdVector.size(); ++i) {
            IMAGE_IAS_RESULT iasResult;
            iasResult.objectId    = imageGroup.objectId;
            iasResult.cameraId    = imageGroup.cameraIdVector[i];
            iasResult.imageId     = imageGroup.imageIdVector[i];
            iasResult.iasType     = IAS_AI;
            iasResult.channelID   = m_channelID;
            iasResult.result      = RESULT_TIMEOUT;
            iasResult.jsonData    = QByteArray();
            Q_EMIT imageIasResultSignal(iasResult);
        }
        return;
    }

    for (size_t i = 0; i < (size_t)imageGroup.cameraIdVector.size(); ++i) {
        cameraIdVector.push_back(imageGroup.cameraIdVector[i]);
        imageDataVector.push_back(imageGroup.imageDataVector[i]);
    }

//    qint64 infer_begin = QDateTime::currentMSecsSinceEpoch();
    resultVector = m_aiInference->infer(cameraIdVector, imageDataVector);
//    qint64 infer_end = QDateTime::currentMSecsSinceEpoch();

//    qDebug().noquote() << QString("obj = %1, infer cost = %2").arg(imageGroup.objectId).arg(infer_end - infer_begin);

    QVector<QByteArray> byteArrayVector = resultVectorToByteArray(cameraIdVector, resultVector);

    for (size_t i = 0; i < (size_t)imageGroup.cameraIdVector.size(); ++i) {
        IMAGE_IAS_RESULT iasResult;
        iasResult.objectId     = imageGroup.objectId;
        iasResult.cameraId     = imageGroup.cameraIdVector[i];
        iasResult.imageId      = imageGroup.imageIdVector[i];
        iasResult.iasType      = IAS_AI;
        iasResult.channelID    = m_channelID;

        if (resultVector[i].getResult()) {
            iasResult.result   = RESULT_OK;
        } else {
            iasResult.result   = RESULT_NG;
        }
        iasResult.jsonData = byteArrayVector[i];
        Q_EMIT imageIasResultSignal(iasResult);
    }
}

QVector<QByteArray> BatchInfer::resultVectorToByteArray(const std::vector<int> &cameraIdVector, const std::vector<BaseAIResult> &resultVector)
{
    QVector<QByteArray> byteArrayVec;

    for (size_t i = 0; i < cameraIdVector.size(); ++i) {
        QJsonArray  pos_infoJsonArray;
        QJsonArray  isg_infoJsonArray;
        QJsonObject resultJson;

        resultJson.insert("channel_id", cameraIdVector[i]);          //相机ID
        resultJson.insert("is_label_show", false);

        BaseAIResult baseResult = resultVector[i];

        QJsonDocument posDoc = QJsonDocument::fromJson(QString::fromStdString(baseResult.getPositionInfo()).toLocal8Bit());
        pos_infoJsonArray.append(posDoc.object());
        resultJson.insert("pos_info", pos_infoJsonArray);

        QJsonDocument isgDoc = QJsonDocument::fromJson(QString::fromStdString(baseResult.getIsgInfo()).toLocal8Bit());
        isg_infoJsonArray.append(isgDoc.object());
        resultJson.insert("isg_info", isg_infoJsonArray);

        resultJson.insert("algo_type", IAS_AI);
        QByteArray result = QJsonDocument(resultJson).toJson(QJsonDocument::Compact);

        byteArrayVec.append(result);
    }
    return byteArrayVec;
}

bool BatchInfer::onOperateInfer(int op_type, const QJsonObject &inputJson, QJsonObject &resultJson)
{
    (void)resultJson;
    int checksum;

    switch (op_type) {
        case AI_OPERATION_ADD:
            return addAiInference(inputJson);

        case AI_OPERATION_UPDATE_PARAM:{
                QJsonObject params = inputJson.value("params").toObject();
                if (m_aiInference == nullptr) {
                    return false;
                } else {
                    return m_aiInference->update(IasJsonParser::toQString(params).toStdString());
                }
            }

        case AI_OPERATION_SAVE:
            if (saveAlgorithmFile(inputJson, checksum)) {
                resultJson.insert("channel_id", m_channelID);
                resultJson.insert("json_checksum", checksum);
                return true;
            }
            break;

        default:
            qInfo().noquote() << "error operation type: " << op_type;
            break;
    }

    return false;
}

bool BatchInfer::addAiInference(const QJsonObject &inputJson)
{
    int componentType = IasJsonParser::getComponentType(inputJson);
    int componentId   = IasJsonParser::getComponentId(inputJson);

    if (m_aiInference != nullptr) {
        qInfo().noquote() << "Add ai, ai is loaded, can not add!";
        return false;
    }

    BaseAiInferenceFactory *baseAiInferenceFactory = FindBaseAiInferenceFactory(componentType);
    if (baseAiInferenceFactory == nullptr) {
        qInfo().noquote() << "Add ai, componentType " << componentType << "is not found";
        return false;
    }
    m_aiInference = baseAiInferenceFactory->getAiInference();
    m_aiInference->setId(componentId);
    m_aiInference->setPath(m_algorithmJsonFilePath.toStdString());
/*
    for (int i = 0; i < m_cameraIdList.size(); ++i)
    {
        m_aiInference->addCameraId(m_cameraIdList.at(i));
    }
*/
    QJsonObject params = inputJson.value("params").toObject();
    bool ret = m_aiInference->load(IasJsonParser::toQString(params).toStdString());
    if (ret) {
        m_bInitOK = true;
    }
    return ret;
}

bool BatchInfer::saveAlgorithmFile(const QJsonObject &jsonFileObj, int &checksum)
{
    QFile file(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);
    int sum = 0;

    if (file.open(QIODevice::WriteOnly)) {
        QByteArray byteArray = QJsonDocument(jsonFileObj).toJson();
        for (int i = 0; i < byteArray.size(); ++i) {
            sum += byteArray[i];
        }
        file.write(byteArray);
        file.flush();
        file.close();
        checksum = sum;
        return true;
    }
    return false;
}

int BatchInfer::getAlgorithmFileChecksum()
{
    QFile file(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);
    int sum = 0;

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray byteArray = file.readAll();
        for (int i = 0; i < byteArray.size(); ++i) {
            sum += byteArray[i];
        }
        file.close();
    }
    return sum;
}

void RegisterBaseAiInferenceFactory(int componentType, BaseAiInferenceFactory *aiInferenceFactory)
{
    baseAiInferenceFactoryHash.insert(componentType, aiInferenceFactory);
}

BaseAiInferenceFactory *FindBaseAiInferenceFactory(int componentType)
{
    QHash<int, BaseAiInferenceFactory *>::const_iterator iterator;

    iterator = baseAiInferenceFactoryHash.find(componentType);
    if (iterator != baseAiInferenceFactoryHash.end()) {
        return iterator.value();
    }
    return nullptr;
}
