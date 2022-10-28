#include "channelInfer.h"
#include "../BaseLib/EngineProject.h"
#include "../ImgSrc/ImageSrcDev.h"
#include <QtConcurrent>
#include <QFuture>
#include <QDebug>
#include <QHash>

QHash<int, BaseLocatorFactory *> baseLocatorFactoryHash;
QHash<int, BaseDetectorFactory *> baseDetectorFactoryHash;

ChannelInfer::ChannelInfer(int channel_id, EngineProject *project):
    m_project(project),
    m_algorithmJsonParser(nullptr),
    m_channelID(channel_id),
    m_bInitOK(false),
    m_bGroup(false),
    m_jsonChecksum(0)
{
    m_algorithmJsonFileName = "AlgoConfig.json";
}

bool ChannelInfer::init()
{
    for (auto &devices : ImageSrcDev::devices()) {
        if (devices->id() == m_channelID) {
            //订阅点检相机和实体相机的图片数据信号
            connect(devices, SIGNAL(imageDataSignal(const IMAGE_DATA &)), this, SLOT(onImageData(const IMAGE_DATA &)));
        }
    }

    moveToThread(&m_thread);
    m_thread.setObjectName(QString("ChannelInfer_%1").arg(m_channelID));
    m_thread.start();

    return true;
}

//执行文件的路径\algorithm\牌号\DT_xx\AlgoConfig.jso
bool ChannelInfer::loadAlgorithmConfig(const QString &algorithmJsonFilePath)
{
    m_algorithmJsonFilePath = algorithmJsonFilePath + QString("/DT_%1").arg(m_channelID);
    m_jsonChecksum = getAlgorithmFileChecksum();

    m_algorithmJsonParser = new IasJsonParser(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);
    QMetaObject::invokeMethod(this, "releaseModel");
    QMetaObject::invokeMethod(this, "initModel");
    return true;
}

bool ChannelInfer::initModel()
{
    QJsonObject jsonobject;

    addLocator(jsonobject);  // 创建缺省Locator
    for (int i = 0; i < getAlgoChannelLocatorsNum(); ++i) {
        addLocator(getAlgoChannelLocator(i));
    }

    for (int i = 0; i < getAlgoChannelDetectorsNum(); ++i) {
        addDetector(getAlgoChannelDetector(i));
    }
    return true;
}

void ChannelInfer::cleanup()
{
    m_thread.quit();
    m_thread.wait();

    releaseModel();
}

void ChannelInfer::releaseModel()
{
    QHash<int, ChannelLocator *> ::iterator locatorIter = m_locators.begin();
    QHash<int, ChannelDetector *> ::iterator detectorIter = m_detectors.begin();

    ChannelLocator *channelLocator;
    ChannelDetector *channelDetector;

    while (locatorIter !=  m_locators.end())
    {
        channelLocator = locatorIter.value();
        channelLocator->m_baseLocator->remove();
        delete channelLocator->m_baseLocator;
        delete channelLocator;
        ++locatorIter;
    }
    m_locators.clear();

    while (detectorIter !=  m_detectors.end())
    {
        channelDetector = detectorIter.value();
        channelDetector->m_baseDetector->remove();
        delete channelDetector->m_baseDetector;
        delete channelDetector;
        ++detectorIter;
    }
    m_detectors.clear();

    m_bInitOK = false;
}

void ChannelInfer::onImageData(const IMAGE_DATA &imageData)
{
    if (!m_bInitOK)
    {
        return;
    }
    QtConcurrent::run(this, &ChannelInfer::inferImage, imageData);
}

void ChannelInfer::inferImage(const IMAGE_DATA &imageData)
{
    //支撑龙岩左右通道业务，烟包可能属于左通道，也可能右通道，根据特征检测算法，确定烟条归属的通道
    bool result[2][2];//group + feature
    QHash<int, BaseDTResult> locator_results[2]; //group
    QHash<int, BaseDTResult> detector_results[2]; //group
    IMAGE_IAS_RESULT output;

    output.objectId  = imageData.objectId;
    output.imageId   = imageData.imageId;
    output.cameraId  = imageData.cameraId;
    output.iasType   = IAS_DT;
    output.channelID = m_channelID;
    output.result    = RESULT_OK;   // 图片的result默认是OK

    for (int group = 0; group < 2; ++group) {
        for (int feature = 0; feature < 2; ++feature) {
            result[group][feature] = true;
        }
    }

    for (int group = 0; group < 2; ++group) {
        for (int feature = 0; feature < 2; ++feature)
        {
            if (!result[group][0])
            {
                break; //特征检测失败，不进行非特征的检测
            }
            // 定位检测
            QVector<QFuture<BaseDTResult>> locate_future;
            for (auto i = m_locators.constBegin(); i != m_locators.constEnd(); i++) {
                if ((i.value()->m_group == group + 1) && (i.value()->m_feature == feature + 1)) {
                    locate_future.append(QtConcurrent::run(this, &ChannelInfer::locate, i.key(), imageData.imageDataCV));
                }
            }
            for (int i = 0; i < locate_future.size(); ++i) {
                if (!locate_future[i].isFinished()) {
                    locate_future[i].waitForFinished();
                }
                BaseDTResult r = locate_future[i].result();
                locator_results[group].insert(r.getId(), r);

                if (r.getId() == DEFAULT_LOCATE_ID) { //缺省locator定位结果供各group使用
                    locator_results[1].insert(r.getId(), r);
                }

                if (!r.getResult()) {
                    result[group][feature] = false;
                }
            }
            if (result[group][feature]) {
                // 2 推理检测
                QVector<QFuture<BaseDTResult>> detect_future;
                for (auto i = m_detectors.constBegin(); i != m_detectors.constEnd(); ++i) {
                    if ((i.value()->m_group == group + 1) && (i.value()->m_feature == feature + 1)) {
                        QFuture<BaseDTResult> f = QtConcurrent::run(this, &ChannelInfer::detect, i.key(), imageData.imageDataCV, locator_results[group]);
                        detect_future.append(f);
                    }
                }
                for (int i = 0; i < detect_future.size(); ++i) {
                    if (!detect_future[i].isFinished()) {
                        detect_future[i].waitForFinished();
                    }
                    BaseDTResult r = detect_future[i].result();
                    detector_results[group].insert(r.getId(), r);
                    if (!r.getResult())
                    {
                        result[group][feature] = false;
                    }
                }
            }
        } //end for feature

        if (result[0][0] || !m_bGroup) //第一组特征检测正确，以第一组为检测结果
        {
            if (result[0][0] && result[0][1]) {
                output.result = RESULT_OK;
            } else {
                output.result = RESULT_NG;
            }
            output.jsonData = resultJsonToByteArray(locator_results[0], detector_results[0], 0);

            Q_EMIT imageIasResultSignal(output);
            return;
        }
    } // end for group

    //第一组特征检测有错误，以第二组为检测结果
    if (result[1][0] && result[1][1]) {
        output.result = RESULT_OK;
    } else {
        output.result = RESULT_NG;
    }
    output.jsonData = resultJsonToByteArray(locator_results[1], detector_results[1], 1);
    Q_EMIT imageIasResultSignal(output);
}

BaseDTResult ChannelInfer::locate(int locator_id, const cv::Mat &mat)
{
    BaseDTResult baseResult;
    QHash<int, ChannelLocator *>::const_iterator iter;

    iter = m_locators.find(locator_id);

    if (iter != m_locators.end()) {
        return iter.value()->m_baseLocator->locate(mat);
    } else {
        qInfo().noquote() << "locate error: can not find locator";
        baseResult.setResult(false);
        return baseResult;
    }
}

BaseDTResult ChannelInfer::detect(int detector_id, const cv::Mat &mat, const QHash<int, BaseDTResult> &locator_results)
{
    int id_x = m_detectors[detector_id]->m_baseDetector->getLocateIdX();
    int id_y = m_detectors[detector_id]->m_baseDetector->getLocateIdY();

    cv::Point2d p(locator_results[id_x].getLocatePoint().x, locator_results[id_y].getLocatePoint().y);

    return m_detectors[detector_id]->m_baseDetector->detect(mat, p);
}

QByteArray ChannelInfer::resultJsonToByteArray(const QHash<int, BaseDTResult> &locator_results, const QHash<int, BaseDTResult> &detector_results, int group)
{
    QJsonArray  postionInfo;
    QJsonObject resultJson;

    for (auto i = locator_results.constBegin(); i != locator_results.constEnd(); i++) {
        if (i.key() == DEFAULT_LOCATE_ID) {
            continue;
        }
        BaseDTResult baseResult = i.value();
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(baseResult.getPositionInfo()).toLocal8Bit());
        postionInfo.append(doc.object());
    }

    for (auto i = detector_results.begin(); i != detector_results.end(); i++) {
        BaseDTResult baseResult = i.value();
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(baseResult.getPositionInfo()).toLocal8Bit());
        postionInfo.append(doc.object());
    }

    resultJson.insert("channel_id", m_channelID);
    resultJson.insert("is_label_show", false);
    resultJson.insert("group", group + 1);
    resultJson.insert("pos_info", postionInfo);
    resultJson.insert("algo_type", IAS_DT);

    QJsonDocument doc = QJsonDocument(resultJson);
    QByteArray byteArray = doc.toJson(QJsonDocument::Compact);

    return byteArray;
}

bool ChannelInfer::addLocator(const QJsonObject &inputJson)
{
    BaseLocator *baseLocator;
    int method;
    int id;

    if (inputJson.isEmpty()) {
        method = DEFAULT_LOCATE_METHOD;
        id = DEFAULT_LOCATE_ID;
    } else {
        method = IasJsonParser::getMethod(inputJson);
        id = IasJsonParser::getId(inputJson);
    }

    if (m_locators.contains(id)) {
        qInfo() << QString("camera %1 add Locator %2, locator is existed, can not add!").arg(m_channelID).arg(id);
        return false;
    }

    BaseLocatorFactory *baseLocatorFactory = FindBaseLocatorFactory(method);
    if (baseLocatorFactory == nullptr) {
        qInfo().noquote() << QString("camera %1 add locator, locator method %2 is not found").arg(m_channelID).arg(method);
        return false;
    } else {
        qInfo().noquote() << QString("camera %1 add locator, locator method %2 is found").arg(m_channelID).arg(method);
    }
    baseLocator = baseLocatorFactory->getLocator();
    baseLocator->setId(id);
    baseLocator->setPath(m_algorithmJsonFilePath.toStdString());
    try {
        baseLocator->load(IasJsonParser::toQString(inputJson).toStdString());
    } catch (const std::exception &e) {
        delete baseLocator;
        qInfo() << QString("camera %1 add Locator %2, exception: %3").arg(m_channelID).arg(id).arg(e.what());
        return false;
    }
    ChannelLocator *channelLocator = new ChannelLocator();

    channelLocator->m_baseLocator = baseLocator;
    channelLocator->m_group       = IasJsonParser::getGroup(inputJson);
    channelLocator->m_feature     = IasJsonParser::getFeature(inputJson);
    channelLocator->m_method      = method;
    channelLocator->m_id          = id;
    m_locators.insert(id, channelLocator);
    m_bInitOK = true;
    if (channelLocator->m_group > 1) {
        m_bGroup = true;
    }
    return true;
}

bool ChannelInfer::getLocatePoint(const QJsonObject &inputJson, cv::Point2d &locatePoint)
{
    int locateIdX = IasJsonParser::getLocateIdX(inputJson);
    int locateIdY = IasJsonParser::getLocateIdY(inputJson);

    QHash<int, ChannelLocator *>::const_iterator iterX;
    QHash<int, ChannelLocator *>::const_iterator iterY;

    iterX = m_locators.find(locateIdX);
    iterY = m_locators.find(locateIdY);

    if (iterX == m_locators.end() || iterY == m_locators.end())
    {
        return false;
    }
    BaseLocator *baseLocatorX = iterX.value()->m_baseLocator;
    BaseLocator *baseLocatorY = iterY.value()->m_baseLocator;

    locatePoint.x = baseLocatorX->getLocateRoot().x;
    locatePoint.y = baseLocatorY->getLocateRoot().y;
    return true;
}

bool ChannelInfer::getLocatePoint(int IdX, int IdY, cv::Point2d &locatePoint)
{
    QHash<int, ChannelLocator *>::const_iterator iterX;
    QHash<int, ChannelLocator *>::const_iterator iterY;

    iterX = m_locators.find(IdX);
    iterY = m_locators.find(IdY);

    if (iterX == m_locators.end() || iterY == m_locators.end())
    {
        qInfo().noquote() << QString("can not get LocatePoint!");

        return false;
    }
    BaseLocator *baseLocatorX = iterX.value()->m_baseLocator;
    BaseLocator *baseLocatorY = iterY.value()->m_baseLocator;

    locatePoint.x = baseLocatorX->getLocateRoot().x;
    locatePoint.y = baseLocatorY->getLocateRoot().y;
    return true;
}

bool ChannelInfer::addDetector(const QJsonObject &inputJson)
{
    BaseDetector *baseDetector;
    cv::Point2d locate_point;
    int method = IasJsonParser::getMethod(inputJson);
    int id = IasJsonParser::getId(inputJson);

    if (m_detectors.contains(id)) {
        qInfo() << QString("camera %1 add Detector %2, detector is existed, can not add!").arg(m_channelID).arg(id);
        return false;
    }

    BaseDetectorFactory *baseDetectorFactory = FindBaseDetectorFactory(method);
    if (baseDetectorFactory == nullptr) {
        qInfo().noquote() << QString("camera %1 add detector %2, detector method %3 is not found").arg(m_channelID).arg(id).arg(method);
        return false;
    } else {
        qInfo().noquote() << QString("camera %1 add detector %2, detector method %3 is found").arg(m_channelID).arg(id).arg(method);
    }

    if (!getLocatePoint(inputJson, locate_point)) {
        qInfo().noquote() << QString("camera %1 add detector %2, can not get locator").arg(m_channelID).arg(id);
        return false;
    }
    baseDetector = baseDetectorFactory->getDetector();
    baseDetector->setId(id);
    baseDetector->setPath(m_algorithmJsonFilePath.toStdString());
    try {
        baseDetector->load(IasJsonParser::toQString(inputJson).toStdString(), locate_point);
    } catch (const std::exception &e) {
        delete baseDetector;
        qInfo() << QString("camera %1 add Detector %2, exception: %3").arg(m_channelID).arg(id).arg(e.what());
        return true;
    }
    ChannelDetector *channelDetector = new ChannelDetector();

    channelDetector->m_baseDetector = baseDetector;
    channelDetector->m_group        = IasJsonParser::getGroup(inputJson);
    channelDetector->m_feature      = IasJsonParser::getFeature(inputJson);
    channelDetector->m_method       = method;
    channelDetector->m_id           = id;
    m_detectors.insert(id, channelDetector);
    if (channelDetector->m_group > 1) {
        m_bGroup = true;
    }
    m_bInitOK = true;
    return true;
}

bool ChannelInfer::removeLocator(const QJsonObject &inputJson)
{
    int id = IasJsonParser::getId(inputJson);
    ChannelLocator *channelLocator;

    if (m_locators.contains(id)) {
        channelLocator = m_locators.find(id).value();
        channelLocator->m_baseLocator->remove();
        m_locators.remove(id);
        delete channelLocator->m_baseLocator;
        delete channelLocator;
    } else {
        qInfo() << QString("camera %1 remove Locator %2, locator is not existed!").arg(m_channelID).arg(id);
        return false;
    }
    return true;
}

bool ChannelInfer::removeDetector(const QJsonObject &inputJson)
{
    int id = IasJsonParser::getId(inputJson);
    ChannelDetector *channelDetector;

    if (m_detectors.contains(id)) {
        channelDetector = m_detectors.find(id).value();
        channelDetector->m_baseDetector->remove();
        m_detectors.remove(id);

        delete channelDetector->m_baseDetector;
        delete channelDetector;
    } else {
        qInfo() << QString("camera %1 remove Detector %2, detector is not existed!").arg(m_channelID).arg(id);
        return false;
    }
    return true;
}

bool ChannelInfer::updateLocator(const QJsonObject &inputJson)
{
    int id = IasJsonParser::getId(inputJson);
    bool result = false;

    if (m_locators.contains(id)) {
        try {
            ChannelLocator *channelLocator;
            BaseLocator *baseLocator;

            channelLocator = m_locators.find(id).value();
            baseLocator = channelLocator->m_baseLocator;

            result = baseLocator->update(IasJsonParser::toQString(inputJson).toStdString());
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 update Locator exception: %2").arg(m_channelID).arg(e.what());
            return false;
        }
    }
    if (result == true) {
        for (auto i = m_detectors.constBegin(); i != m_detectors.constEnd(); i++) {
            ChannelDetector *channelDetector = i.value();
            BaseDetector *baseDetector = channelDetector->m_baseDetector;
            cv::Point2d localPoint;
            if (getLocatePoint(baseDetector->getLocateIdX(), baseDetector->getLocateIdY(), localPoint)) {
                baseDetector->setLocateRoot(localPoint);
            } else {
                qInfo().noquote() << "updateLocator, detector can not find locatePoint";
            }
        }
    }
    return result;
}

bool ChannelInfer::updateDetector(const QJsonObject &inputJson)
{
    int id = IasJsonParser::getId(inputJson);
    QHash<int, ChannelDetector *>::const_iterator iter;

    iter = m_detectors.find(id);
    if (iter != m_detectors.end()) {
        BaseDetector *baseDetector = iter.value()->m_baseDetector;

        cv::Point2d locatePoint;
        if (!getLocatePoint(inputJson, locatePoint)) {
            qInfo().noquote() << "updateDetector, detector can not find locatePoint";
            return false;
        }

        baseDetector->setLocateRoot(locatePoint);
        try {
            return baseDetector->update(IasJsonParser::toQString(inputJson).toStdString());
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 update Detector exception: %2").arg(m_channelID).arg(e.what());
        }
    }
    return false;
}

bool ChannelInfer::resetLocator(const QJsonObject &inputJson, QJsonObject &resultJson)
{
    int id = IasJsonParser::getId(inputJson);
    bool result;

    if (m_locators.contains(id)) {
        try {
            std::string resultStdString;

            result = m_locators.find(id).value()->m_baseLocator->reset(resultStdString);
            if (result) {
                QString resultQString = QString::fromStdString(resultStdString);
                resultJson = QJsonDocument::fromJson(resultQString.toLocal8Bit()).object();
                return true;
            }
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 reset Locator %2 exception: %3").arg(m_channelID).arg(id).arg(e.what());
        }
    }
    return false;
}

bool ChannelInfer::resetDetector(const QJsonObject &inputJson, QJsonObject &resultJson)
{
    int id = IasJsonParser::getId(inputJson);
    bool result;

    if (m_detectors.contains(id)) {
        try {
            std::string resultStdString;
            result = m_detectors.find(id).value()->m_baseDetector->reset(resultStdString);
            if (result) {
                QString resultQString = QString::fromStdString(resultStdString);
                resultJson = QJsonDocument::fromJson(resultQString.toLocal8Bit()).object();
                return true;
            }
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 reset Detector %2 exception: %3").arg(m_channelID).arg(id).arg(e.what());
        }
    }
    return false;
}

bool ChannelInfer::execLocator(const QJsonObject &inputJson, QJsonObject &resultJson)
{
    int id = IasJsonParser::getId(inputJson);
    bool result;

    if (m_locators.contains(id)) {
        try {
            std::string resultStdString;

            result = m_locators.find(id).value()->m_baseLocator->exec(resultStdString);
            if (result) {
                QString resultQString = QString::fromStdString(resultStdString);
                resultJson = QJsonDocument::fromJson(resultQString.toLocal8Bit()).object();
                return true;
            }
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 exec Locator %2 exception: %3").arg(m_channelID).arg(id).arg(e.what());
        }
    }
    return false;
}

bool ChannelInfer::execDetector(const QJsonObject &inputJson, QJsonObject &resultJson)
{
    int id = IasJsonParser::getId(inputJson);
    bool result;

    if (m_detectors.contains(id)) {
        try {
            std::string resultStdString;

            result = m_detectors.find(id).value()->m_baseDetector->exec(resultStdString);
            if (result) {
                QString resultQString = QString::fromStdString(resultStdString);
                resultJson = QJsonDocument::fromJson(resultQString.toLocal8Bit()).object();
                return true;
            }
        } catch (const std::exception &e) {
            qInfo().noquote() << QString("camera %1 exec Detector %2 exception: %3").arg(m_channelID).arg(id).arg(e.what());
        }
    }
    return false;
}

bool ChannelInfer::onOperateInfer(int op_type, int dt_type, const QJsonObject &inputJson, QJsonObject &resultJson)
{
    int checksum;

    switch (op_type)
    {
        case DT_OPERATION_ADD:
            if (dt_type == DT_TYPE_LOCATOR) {
                return addLocator(inputJson);
            } else if (dt_type == DT_TYPE_DETECTOR) {
                return addDetector(inputJson);
            }
            break;

        case DT_OPERATION_DEL:
            if (dt_type == DT_TYPE_LOCATOR) {
                return removeLocator(inputJson);
            } else if (dt_type == DT_TYPE_DETECTOR) {
                return removeDetector(inputJson);
            }
            break;

        case DT_OPERATION_UPDATE_PARAM:
            if (dt_type == DT_TYPE_LOCATOR) {
                return updateLocator(inputJson);
            } else if (dt_type == DT_TYPE_DETECTOR) {
                return updateDetector(inputJson);
            }
            break;

        case DT_OPERATION_UPDATE_IMAGE:
            return true;

        case DT_OPERATION_AUTOVERIFY:
            if (dt_type == DT_TYPE_LOCATOR) {
                return resetLocator(inputJson, resultJson);
            } else if (dt_type == DT_TYPE_DETECTOR) {
                return resetDetector(inputJson, resultJson);
            }
            break;

        case DT_OPERATION_PROCESS:
            if (dt_type == DT_TYPE_LOCATOR) {
                return execLocator(inputJson, resultJson);
            } else if (dt_type == DT_TYPE_DETECTOR) {
                return execDetector(inputJson, resultJson);
            }
            break;

        case DT_OPERATION_SAVE:
            if (saveAlgorithmFile(inputJson, checksum)) {
                resultJson.insert("channel_id", m_channelID);
                resultJson.insert("json_checksum", checksum);
                return true;
            }
            break;

        default:
            qInfo().noquote() << QString("camera %1 operate, error operate type: %2").arg(m_channelID).arg(op_type);
            return false;
    }
    return false;
}

int ChannelInfer::getAlgoChannelLocatorsNum()
{
    return m_algorithmJsonParser->get(QString("channel/locators")).toArray().size();
}

int ChannelInfer::getAlgoChannelDetectorsNum()
{
    return m_algorithmJsonParser->get(QString("channel/detectors")).toArray().size();
}

QJsonObject ChannelInfer::getAlgoChannelLocator(int index)
{
    return m_algorithmJsonParser->get(QString("channel/locators/%2").arg(index)).toObject();
}

QJsonObject ChannelInfer::getAlgoChannelDetector(int index)
{
    return m_algorithmJsonParser->get(QString("channel/detectors/%2").arg(index)).toObject();
}

bool ChannelInfer::saveAlgorithmFile(const QJsonObject &jsonFileObj, int &checksum)
{
    QFile file(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);
    int sum = 0;

    if (file.open(QIODevice::WriteOnly))
    {
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

int ChannelInfer::getAlgorithmFileChecksum()
{
    QFile file(m_algorithmJsonFilePath + "/" + m_algorithmJsonFileName);
    int sum = 0;

    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray byteArray = file.readAll();
        for (int i = 0; i < byteArray.size(); ++i) {
            sum += byteArray[i];
        }
        file.close();
    }
    return sum;
}

void RegisterBaseLocatorFactory(int method, BaseLocatorFactory *locatorFactory)
{
    baseLocatorFactoryHash.insert(method, locatorFactory);
}

void RegisterBaseDetectorFactory(int method, BaseDetectorFactory *detectorFactory)
{
    baseDetectorFactoryHash.insert(method, detectorFactory);
}

BaseLocatorFactory *FindBaseLocatorFactory(int method)
{
    QHash<int, BaseLocatorFactory *>::const_iterator iterator;

    iterator = baseLocatorFactoryHash.find(method);
    if (iterator != baseLocatorFactoryHash.end()) {
        return iterator.value();
    }
    return nullptr;
}

BaseDetectorFactory *FindBaseDetectorFactory(int method)
{
    QHash<int, BaseDetectorFactory *>::const_iterator iterator;

    iterator = baseDetectorFactoryHash.find(method);
    if (iterator != baseDetectorFactoryHash.end()) {
        return iterator.value();
    }
    return nullptr;
}
