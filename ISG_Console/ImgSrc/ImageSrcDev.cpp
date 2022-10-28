#include "ImageSrcDev.h"
#include <QDateTime>
#include <QDebug>
#include <functional>
#include <QString>
#include <QDateTime>
#include "BaseLib/EngineProject.h"
#include "AssistLib/AuxTask.h"

int ImageSrcDev::s_spotDetectimageCount = 0;
int ImageSrcDev::s_spotDetectWaitFinishCount = 0;
int ImageSrcDev::s_virtualCameraCount = 0;
QList<ImageSrcDev *> ImageSrcDev::s_imageSrcDevices;
QAtomicInt ImageSrcDev::s_onlineCameraCount;

ImageSrcDev::ImageSrcDev(int id, EngineProject *project) :
    EngineObject(id, project),
    m_serialNumber("serial_number", "", this),
    m_accessCode("access_code", "", this),
    m_gain("gain", -1.0, this),
    m_exposeTime("exposure_time", -1.0, this),
    m_statisticTimer(new QTimer(this)),
    m_online(false),
    m_isSoftCamera(false),
    m_isSpotCamera(false),
    m_imageSaveStrategy("image_save_strategy", IMAGE_SAVE_NONE, this),
    m_imageSaveRatio("image_save_sample", 10, this),
    m_imageSaveFormat("image_save_format", "JPG", this),
    m_captureCompensation("capture_compensation_ms", 0, this),
    m_detectTotalCount(0),
    m_imageSaveRatioIndex(0),
    m_imageReportIndex(0),
    m_imageCountRecordCnt(0)
{
    m_statisticTimer->setInterval(1000);
    connect(m_statisticTimer, SIGNAL(timeout()), SLOT(onStatisticTimeout()));
    s_imageSrcDevices.append(this);
}

ImageSrcDev::~ImageSrcDev()
{
    s_imageSrcDevices.removeOne(this);
}

bool ImageSrcDev::init()
{
    qInfo().noquote() << tr("ImageSrcDev: init ImageData %1(camera id = %2)")
                          .arg(name())
                          .arg(id());

    if ((project()->productType() == PRODUCT_TYPE_BOX) && (id() == 1) && (!m_isSoftCamera))
    {
        //烟丝产品，订阅IO单板的箱成形信号，对箱成形图片进行检测
        connect(PCPUdpCtrl::getInstance(), SIGNAL(boxShapeSignal()), this, SLOT(onBoxShapeSignal()));
    }
    m_statisticTimer->start();

    moveToThread(&m_thread);
    m_thread.setObjectName(name());
    m_thread.start();
    return true;
}

//图像源线程处理时，调用了综合汇总等对象；在cleanup阶段就停止线程，防止在析构阶段，综合汇总对象已经释放了。
void ImageSrcDev::cleanup()
{
    QMetaObject::invokeMethod(m_statisticTimer, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void ImageSrcDev::onStarted()
{
    QMetaObject::invokeMethod(m_statisticTimer, "start");
}

void ImageSrcDev::onStopped()
{
    QMetaObject::invokeMethod(m_statisticTimer, "stop");
}

const QList<ImageSrcDev *> &ImageSrcDev::devices()
{
    return s_imageSrcDevices;
}

const QList<int> ImageSrcDev::getCameraIdList()
{
    QList<int> CameraIdList;

    for (auto &devices : s_imageSrcDevices) {
        if (!devices->isSpotCamera()) {
            CameraIdList.append(devices->id());
        }
    }

    return CameraIdList;
}

void ImageSrcDev::setOnline(bool online)
{
    if (m_online != online) {
        m_online = online;
        if (online) {
            s_onlineCameraCount++;
        } else {
            s_onlineCameraCount--;
        }
    }
}

int ImageSrcDev::getRealOnlineCameraCount(int min, int max)
{
    int count = 0;

    for (auto &device : s_imageSrcDevices) {
        if (!device->isSpotCamera()
                && device->isOnline()
                && (device->id() >= min)
                && (device->id() <= max)) {
            count ++;
        }
    }
    return count;
}

void ImageSrcDev::setSoftCamera(bool isSoftCamera)
{
    m_isSoftCamera = isSoftCamera;
}

void ImageSrcDev::setSpotCamera(bool isSpotCamera)
{
    if (!m_isSpotCamera) {
        m_isSpotCamera = isSpotCamera;
        s_virtualCameraCount ++;
    }
}

void ImageSrcDev::sendImageData(const QImage &image, uint64_t imageGenerateTime, uint64_t fileNameId)
{
    Q_ASSERT(!image.isNull());

    if (!m_enabled.value().toBool()) {
        return;
    }
    if ((project()->productType() == PRODUCT_TYPE_BOX) && (id() == 1) && (!isSoftCamera())) {
        IMAGE_DATA imageData;
        imageData.cameraId = id();
        imageData.imageId = 0;
        imageData.genTimeStamp = imageGenerateTime;
        imageData.imageData = image;
        m_Image = image;
        project()->tcpImageReport()->sendImageDataToPolicyStore(imageData);
        return;
    }
    imageGenerateTime += m_captureCompensation.value().toInt();
    QMetaObject::invokeMethod(this, "sendImageDataAsync",
                                  Q_ARG(QImage, image),
                                  Q_ARG(uint64_t, imageGenerateTime),
                                  Q_ARG(uint64_t, fileNameId));
    if (saveStrategyCheck()) {
        QMetaObject::invokeMethod(this, "onSaveImage", Q_ARG(QImage, image), Q_ARG(uint64_t, imageGenerateTime));
    }
}

void ImageSrcDev::sendImageDataAsync(QImage image, uint64_t imageGenerateTime, uint64_t fileNameId)
{
    if (!m_isSpotCamera) {
        m_detectTotalCount++;
    }

    switch (image.format())
    {
        case QImage::Format_Grayscale8:
        case QImage::Format_BGR888:
        case QImage::Format_RGB32:
            break;
        default: // 其他格式统一转换为BGR888
            qDebug().noquote() << tr("ImageSrcDev: convert image format from %1 to %2")
                                 .arg(image.format())
                                 .arg(QImage::Format_BGR888);
            image = image.convertToFormat(QImage::Format_BGR888);
            break;
    }

    IMAGE_DATA imageData;
    uint64_t imgId = id() | (QDateTime::currentMSecsSinceEpoch() << 8);
    if (m_isSpotCamera) {
        imgId = fileNameId;
    }
    imageData.cameraId = id();
    imageData.imageId = imgId;
    imageData.genTimeStamp = imageGenerateTime;
    imageData.imageData = image;
    imageData.imageDataCV = AuxTask::QImageToCvMat(imageData.imageData);
    //点检模式 不需要做成包判断
    if (project()->getRunType() == RunType_Spot)
    {
        imageData.objectId = QDateTime::currentMSecsSinceEpoch();
        imageData.bReportOrignal = true;
        imageData.bReportBinning = false;
        qInfo().noquote() << tr("Image create: camera id = %1, image-id = %2 Imagetime = %3, Now = %4").arg(id()).arg(imgId).arg(imageGenerateTime).arg(QDateTime::currentMSecsSinceEpoch());
    } else {
        project()->objectResult()->preInfer(imageData);
        qInfo().noquote() << QString("Image create: camera id = %1, image-id = %2 Imagetime = %3, object-id = %4, Now = %5")
                             .arg(id())
                             .arg(imgId)
                             .arg(imageGenerateTime)
                             .arg(imageData.objectId)
                             .arg(QDateTime::currentMSecsSinceEpoch());
    }
    emit imageDataSignal(imageData);
}

bool ImageSrcDev::saveStrategyCheck()
{
    int strategy = m_imageSaveStrategy.value().toInt();
    int ratio = m_imageSaveRatio.value().toInt();

    //不保存任何图像
    if (strategy == IMAGE_SAVE_NONE) {
        return false;
    }
    //保存全部错误和全部正确图像
    if (strategy == IMAGE_SAVE_ALL) {
        return true;
    }
    //图像按比例进行保存
    m_imageSaveRatioIndex++;
    if (m_imageSaveRatioIndex >= ratio)
    {
        m_imageSaveRatioIndex = 0;
        return true;
    }
    return false;
}

void ImageSrcDev::onSaveImage(const QImage &image, uint64_t imageTime)
{
    QStringList filePath = makeImageName(imageTime);

    project()->m_pAuxTask->cleanImageDirectoryForNoSpace(project()->imageSavePath());
    project()->m_pAuxTask->saveImage(image, filePath[0], filePath[1], filePath[2]);
}

QStringList ImageSrcDev::makeImageName(uint64_t imageTime)
{
    QString path = project()->imageSavePath() + "/" + QString::number(id(), 10);
    QDateTime time = QDateTime::fromMSecsSinceEpoch(imageTime);
    QString dir = "/AllImg";

    QStringList filePath;
    filePath << path + dir + time.toString("/yyyyMMdd/hh/");
    filePath << QString::number(id()) + "#" + time.toString("yyyyMMddhhmmsszzz");
    filePath << m_imageSaveFormat.value().toString();
    return filePath;
}

void ImageSrcDev::onStatisticTimeout()
{
    //针对非点检相机，每5分钟向日志文件记录一下拍照数目
    m_imageCountRecordCnt++;
    if ((m_imageCountRecordCnt > 300) && (!m_isSpotCamera)) {
//        qInfo() << tr("camera-%1; photo number: %2 ").arg(id()).arg(m_detectTotalCount);
        m_imageCountRecordCnt = 0;
    }
}

void ImageSrcDev::onBoxShapeSignal()
{
    if (!m_Image.isNull() ) {
        sendImageDataAsync(m_Image, QDateTime::currentMSecsSinceEpoch(), 0);
    }
}

bool ImageSrcDev::onSerialNumberChange(const QString &, const QString &, const bool &trusted)
{
    return trusted;
}

bool ImageSrcDev::onGainRawChange(const double &, const double &, const bool &trusted)
{
    return trusted;
}

bool ImageSrcDev::onExposeTimeChange(const double &, const double &, const bool &trusted)
{
    return trusted;
}

bool ImageSrcDev::setTrggerMode()
{
    return true;
}
