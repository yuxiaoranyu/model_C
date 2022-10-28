#include "EngineProject.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "ImgSrc/ImageSrcDev.h"
#include "Multipler/PacketResult.h"
#include "Multipler/BoxResult.h"
#include "IasAgent/iasInfer.h"
#include "IasCore/myAiInference.h"
#include "IasCore/myLocator.h"
#include "IasCore/myDetector.h"

QHash<QString, ObjectCreator> EngineProject::s_creator;

EngineProject::EngineProject() :
    EngineObject(this),
    m_def("def", "", this, false),
    m_params("params", "", this, false),
    m_productType("product", "", this, false),
    m_algorithmType("algorithm_type", IAS_DT_AI, this, false),
    m_brandId("brand_id", 0, this),
    m_save("save", false, this, false),
    m_imageSavePath("image_save_path", "/opt", this),
    m_imageFileKeepDays("image_file_keep_days", 20, this),
    m_logFileKeepDays("log_file_keep_days", 60, this),
    m_policyStoreIp("policystore_ip", "", this),
    m_policyStorePort("policystore_port", 1001, this),
    m_imageMaxInterval("image_max_interval", 30, this),
    m_imageInferTimeoutMS("image_infer_timeout_ms", 200, this),
    m_objectInferTimeoutMS("object_infer_timeout_ms", 200, this),
    m_boxQueueLen("box_queue_len", 3, this, true, PRODUCT_TYPE_BOX),
    m_boxMinElapsed("box_min_elapsed", 2000, this, true, PRODUCT_TYPE_BOX),
    m_originalImageReportRate("image_orginal_rate", -1, this),
    m_binningImageReportRate("image_binning_rate", -1, this),
    m_tcpClient(this),
    m_tcpImageReport(this),
    m_realAlarm(this),
    m_objectResult(nullptr),
    m_runType(RunType_Infer),
    m_isRegistaredFlag(false),
    m_isPolicyStoreOnline(false)
{
    m_name.setValue("global", true);
    m_def.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        if (trusted)
        {
            return openDef(value.toString());
        }
        else
        {
            return false;
        }
    });
    m_params.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        if (trusted)
        {
            return loadParams(value.toString());
        }
        else
        {
            return false;
        }
    });
    m_save.defineSetter([this](const QJsonValue &, const QJsonValue &, bool) {
        return saveParams();
    });
    m_imageMaxInterval.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        (void)this;
        if (trusted)
        {
            // 启动时必须大于0才能通过
            return value.toInt() > 0;
        }
        else
        {
            return false;
        }
    });

    m_originalImageReportRate.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        (void)this;
        (void)trusted;
        if (objectResult() != nullptr) {
            objectResult()->originalReportRateSet(value.toInt());
        }
        return true;
    });

    m_binningImageReportRate.defineSetter([this](const QJsonValue &value, const QJsonValue &, bool trusted) {
        (void)this;
        (void)trusted;
        if (objectResult() != nullptr) {
            objectResult()->binningReportRateSet(value.toInt());
        }
        return true;
    });
}

EngineProject::~EngineProject()
{
    cleanup();
}

bool EngineProject::openDef(const QString &path)
{
    if (path.isEmpty()) {
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        return false;
    }
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    QJsonObject obj = doc.object();
    if (obj.isEmpty()) {
        return false;
    }
    m_productType.setValue(obj.value("product").toString());
    m_algorithmType.setValue(obj.value("algorithm_type").toInt());

    if (productType() == PRODUCT_TYPE_BOX) {
        m_objectResult = new CBoxResult(this);
    } else {
        m_objectResult = new CPacketResult(this);
    }

    // 创建相机
    if (!loadDefNode("camera", obj.value("camera"))) {
        qInfo().noquote() << tr("EngineProject: All camera load failed");
        return false;
    }

    //创建条码器
    if (productType() == PRODUCT_TYPE_BOX) {
        if (!loadDefNode("sensor", obj.value("sensor"))) {
            return false;
        }
    }

    // 创建剔除机构
    if (!loadDefNode("executor", obj.value("executor"))) {
        return false;
    }

    return true;
}

bool EngineProject::loadDefNode(const QString &nodeName, const QJsonValue &json)
{
    if (json.type() == QJsonValue::Object) {
        auto name = nodeName;
        auto def = json.toObject();
        int id = 0;
        if (def.contains("id"))
        {
            id = def.value("id").toInt();
            name = tr("%1-%2").arg(name).arg(id);
        }
        QString vendor;
        if (def.contains("vendor"))
        {
            vendor = def.value("vendor").toString();
        }
        else if (def.contains("type"))
        {
            vendor = def.value("type").toString();
        }
        else
        {
            qInfo().noquote() << tr("EngineProject: missing vendor or type for %1").arg(name);
            return false;
        }
        if (m_objects.contains(name))
        {
            qInfo().noquote() << tr("EngineProject: object named %1 exist").arg(name);
            return false;
        }
        auto creator = s_creator.find(vendor);
        if (creator == s_creator.end())
        {
            qInfo().noquote() << tr("EngineProject: can not find creator for %1/%2").arg(vendor).arg(name);
            return false;
        }
        // 创建对象
        ObjectPtr ptr;
        if (!(ptr = (*creator)(id, this)))
        {
            qInfo().noquote() << tr("EngineProject: failed to create %1/%2").arg(vendor).arg(name);
            return false;
        }
        if (nodeName == "camera") {
            CImageMultipler *imageMultipler = new CImageMultipler(id, this);
            m_imageMultiplers.insert(id, imageMultipler);
            if ((getAlgorithmType() == IAS_DT) || (getAlgorithmType() == IAS_DT_AI))
            {
                ChannelInfer *channelInfer = new ChannelInfer(id, this);
                m_channelInfers.insert(id, channelInfer);
            }
            createVirtualCamera(id);
        }
        // 设置对象名称
        ptr->m_name.setValue(name, true);
        m_objects.insert(name, ptr);
    } else if (json.type() == QJsonValue::Array) {
        for (auto item : json.toArray())
        {
            if (!loadDefNode(nodeName, item))
            {
                return false;
            }
        }
    }
    return true;
}

bool EngineProject::loadParams(const QString &path)
{
    if (path.isEmpty()) {
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        return false;
    }
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    QJsonObject obj = doc.object();
    if (obj.isEmpty()) {
        return false;
    }
    // 加载global对象属性
    loadFromJson(obj.value(name()));
    // 加载m_objects对象属性
    for (auto &ptr : m_objects)
    {
        ptr->loadFromJson(obj.value(ptr->name()));
    }


    //    DefaultLocatorInit();
    //    ColorDetectorInit();
    YoloResnetAiInit();

    MyLocatorFactory *myLocatorFactory0 = new MyLocatorFactory();
    RegisterBaseLocatorFactory(0, (BaseLocatorFactory *)myLocatorFactory0);

    MyLocatorFactory *myLocatorFactory1 = new MyLocatorFactory();
    RegisterBaseLocatorFactory(1, (BaseLocatorFactory *)myLocatorFactory1);

    MyDetectorFactory *myDetectorFactory1 = new MyDetectorFactory();
    RegisterBaseDetectorFactory(1, (BaseDetectorFactory *)myDetectorFactory1);

    MyAiInferenceFactory *myAiInferenceFactory0 = new MyAiInferenceFactory();
    RegisterBaseAiInferenceFactory(0, (BaseAiInferenceFactory *)myAiInferenceFactory0);

    //加载AI配置文件
    if ((getAlgorithmType() == IAS_AI) || (getAlgorithmType() == IAS_DT_AI)) {
        loadAI();
    }

    //加载DT配置文件
    for (auto &ptr : m_channelInfers) {
        ptr->loadAlgorithmConfig(QCoreApplication::applicationDirPath() + "/algorithm/" + project()->brandId());
    }

    // 最后对所有对象进行初始化
    return init();
}

void EngineProject::loadAI()
{
    QString brandPath =  QCoreApplication::applicationDirPath() + "/algorithm/" + project()->brandId();
    QDir brandDir(brandPath);
    brandDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
    QDirIterator brandDirIterator(brandDir);
    bool bInt;
    int decValue;
    int count = 0;

    while (brandDirIterator.hasNext()) //遍历各算法目录
    {
        brandDirIterator.next();
        decValue = brandDirIterator.fileName().toInt(&bInt, 10);
        if (bInt)
        {
            AICtrl *pAICtrl = new AICtrl(decValue, this);
            pAICtrl->getBatchInfer()->loadAlgorithmConfig(QCoreApplication::applicationDirPath() + "/algorithm/" + project()->brandId());
            m_AICtrls.insert(decValue, pAICtrl);

            count++;
            if (count >= 6) { //最多允许跑6个AI
                break;
            }

        }
    }
}

bool EngineProject::saveParams()
{
    QString jsonFileName = m_params.value().toString();

    if (jsonFileName.isEmpty()) {
        qDebug().noquote() << tr("EngineProject: path for product_params.json is not set");
        return false;
    }
    if (m_objects.isEmpty()) {
        qInfo().noquote() << tr("EngineProject: no objects to save");
        return false;
    }

    QJsonObject jsonFileObj;
    // 保存对象属性
    jsonFileObj.insert(name(), saveToJson());
    for (auto &ptr : m_objects)
    {
        jsonFileObj.insert(ptr->name(), ptr->saveToJson());
    }
    project()->m_pAuxTask->saveConfigFile(jsonFileName, jsonFileObj);
    return true;
}

void EngineProject::doCommand(const QString &cmd)
{
    static QRegularExpression rx(R"(^\s*([\w-]+)\.(\w+)\s*(?:=\s*(.+)\s*)?$)", QRegularExpression::OptimizeOnFirstUsageOption);
    /*
     * R"()"        表示对之间的内容不做转义
     * ^            匹配字符串开始
     * \s*          匹配任意个数的空格
     * \w           匹配数字、字母和下划线
     * []+          匹配1个或多个特定字符
     * ([\w-]+)     捕获带数字、字母、下划线和减号的【对象名称】
     * \.           匹配小数点
     * (\w+)        捕获带数字、字母、下划线的【属性名称】
     * (?:)         表示之间的内容不计入捕获
     * (?:)?        表示可选的匹配内容
     * .            匹配任意字符
     * (.+)         匹配1个或多个任意字符组成的【属性值】
     * $            匹配字符串结束
     */
    auto m = rx.match(cmd);
    if (m.hasMatch())
    {
        QString name = m.captured(1);
        EngineObject *obj;
        ObjectPtr ptr;
        if (name == this->name())
        {
            obj = this;
        }
        else
        {
            ptr = findObject(name);
            obj = ptr.get();
        }
        if (!obj)
        {
            qDebug().noquote() << tr("EngineProject: object %1 not exist").arg(name);
            return;
        }
        QString key = m.captured(2);
        if (m.lastCapturedIndex() == 2)
        {
            // GET格式: a.b
            auto v = obj->getAttr(key);
            if (!v.isNull())
            {
                qDebug().noquote() << tr("EngineProject: %1.%2 is").arg(name).arg(key) << v;
            }
        }
        else
        {
            // SET格式: a.b = c
            QString value = m.captured(3);
            // 使用json数据类型解析value
            QJsonDocument doc = QJsonDocument::fromJson(tr("[%1]").arg(value).toUtf8());
            if (!doc.isArray())
            {
                qDebug().noquote() << tr("EngineProject: invalid value %1").arg(value);
                return;
            }
            QJsonArray arr = doc.array();
            if (arr.size() != 1)
            {
                qDebug().noquote() << tr("EngineProject: invalid value %1").arg(value);
                return;
            }
            obj->setAttr(key, arr.first());
        }
    }
}

bool EngineProject::init()
{
    static_cast<EngineObject *>(&m_tcpClient)->init();
    static_cast<EngineObject *>(&m_tcpImageReport)->init();
    static_cast<EngineObject *>(&m_realAlarm)->init();

    for (auto &ptr : m_objects)
    {
        if (!ptr->init())
        {
            qInfo().noquote() << tr("EngineProject: failed to init %1").arg(ptr->name());
            return false;
        }
    }
    for (auto &ptr : m_virtualCameras)
    {
        if (!ptr->init())
        {
            qInfo().noquote() << tr("EngineProject: failed to init %1").arg(ptr->name());
            return false;
        }
    }

    //优先订阅imageDataSignal信号，防止推理快速完成而自身尚未接收信号
    for (auto &ptr : m_imageMultiplers)
    {
        if (!ptr->init()) {
            qInfo().noquote() << tr("EngineProject: failed to init mul %1").arg(ptr->id());
            return false;
        }
    }

    for (auto &ptr : m_AICtrls)
    {
        if (!ptr->init())
        {
            qInfo().noquote() << tr("EngineProject: failed to init AICtrl %1").arg(ptr->getChannelId());
            return false;
        }
    }

    for (auto &ptr : m_channelInfers)
    {
        if (!ptr->init())
        {
            qInfo().noquote() << tr("EngineProject: failed to init channel Infer %1").arg(ptr->getChannelId());
            return false;
        }
    }

    m_objectResult->init();

    return true;
}

void EngineProject::onStarted()
{
    qInfo() << "switch to run state";
    if (project()->getRunType() == RunType_Spot)
    {
        for (auto &ptr : m_virtualCameras)  //启动点检相机
        {
            ptr->m_enabled.setValue(true);
        }
    }
    else if (project()->getRunType() == RunType_Infer) //启动实体相机和剔除机构
    {
        for (auto &ptr : m_objects)
        {
            ptr->m_enabled.setValue(true);
        }
    }
}

void EngineProject::onStopped()
{
    qInfo() << "switch to run state";
    if (project()->getRunType() == RunType_Spot) {  //停止点检相机
        for (auto &ptr : m_virtualCameras)
        {
            ptr->m_enabled.setValue(false);
        }
    } else if (project()->getRunType() == RunType_Infer) {
        for (auto &ptr : m_objects)  //停止实体相机和剔除机构
        {
            ptr->m_enabled.setValue(false);
        }
    }
}

void EngineProject::setRunType(const RunTypeEnum runType)
{
    QMutexLocker lock(&m_mutex);
    m_runType = runType;
}

RunTypeEnum EngineProject::getRunType()
{
    QMutexLocker lock(&m_mutex);
    return m_runType;
}

void EngineProject::cleanup()
{
    for (auto &ptr : m_objects) {
        ptr->cleanup();
    }
    m_objects.clear();

    for (auto &ptr : m_virtualCameras) {
        ptr->cleanup();
    }
    m_virtualCameras.clear();

    for (auto &ptr : m_imageMultiplers) {
        ptr->cleanup();
        delete ptr;
    }
    m_imageMultiplers.clear();

    for (auto &ptr : m_AICtrls) {
        ptr->cleanup();
        delete ptr;
    }
    m_AICtrls.clear();

    for (auto &ptr : m_channelInfers) {
        ptr->cleanup();
        delete ptr;
    }
    m_channelInfers.clear();

    // 防止收到数据调用已析构的对象
    static_cast<EngineObject *>(&m_realAlarm)->cleanup();
    static_cast<EngineObject *>(&m_tcpImageReport)->cleanup();
    static_cast<EngineObject *>(&m_tcpClient)->cleanup();

    m_objectResult->cleanup();
    delete m_objectResult;
}

void EngineProject::createVirtualCamera(const int &id)
{
    QString name = QString("virtual-%1").arg(id);
    auto creator = s_creator.find("soft");
    if (creator == s_creator.end())
    {
        qInfo().noquote() << "EngineProject: can not find creator for soft";
        return;
    }

    ObjectPtr ptr;
    if (!(ptr = (*creator)(id, this)))
    {
        qInfo().noquote() << tr("EngineProject: failed to create %1").arg(name);
        return;
    }
    ptr->m_name.setValue(name, true);
    m_virtualCameras.insert(name, ptr);
}
