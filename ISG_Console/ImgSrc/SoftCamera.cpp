#include "SoftCamera.h"
#include <QDateTime>
#include <QDirIterator>
#include <QImage>
#include "BaseLib/EngineProject.h"
#include <QDebug>

SoftCamera::SoftCamera(int id, EngineProject *project) :
    ImageSrcDev(id, project),
    m_timer(new QTimer(this)),
    m_imageSrcPath("image_path", "", this),
    m_imageReadIndex(0),
    m_bStartStage(true),
    m_startDelay("start_delay", 5, this),
    m_imageInterval("interval", 200, this),
    m_isLoop("loop", false, this)
{
    setSoftCamera(true);
    m_timer->setTimerType(Qt::PreciseTimer);

    //重写虚拟相机的定时间间隔的set方法
    m_name.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool) {
        QMetaObject::invokeMethod(this, "setCameraNameAsync",
                                  Q_ARG(QString, newValue.toString()),
                                  Q_ARG(QString, oldValue.toString()));
        return true;
    });

    //重写虚拟相机的图像源的文件路径的set方法
    m_imageSrcPath.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool trusted) {
        QMetaObject::invokeMethod(this, "setImageSrcPathAsync",
                                  Q_ARG(QString, newValue.toString()),
                                  Q_ARG(QString, oldValue.toString()),
                                  Q_ARG(bool, trusted));
        return true;
    });

    //重写虚拟相机的定时间间隔的set方法
    m_imageInterval.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool) {
        QMetaObject::invokeMethod(this, "setIntervalAsync",
                                  Q_ARG(int, newValue.toInt()),
                                  Q_ARG(int, oldValue.toInt()));
        return true;
    });

    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
    setOnline(true);
}

void SoftCamera::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    ImageSrcDev::cleanup();
}

void SoftCamera::onStarted()
{
    m_bStartStage = true;
    m_timer->setInterval(m_startDelay.value().toInt());

    QMetaObject::invokeMethod(m_timer, "start");
}

void SoftCamera::onStopped()
{
    QMetaObject::invokeMethod(m_timer, "stop");
}

void SoftCamera::setCameraNameAsync(const QString &val, const QString &old)
{
    Q_UNUSED(val)
    Q_UNUSED(old)

    if (val.contains("virtual")) {
        setSpotCamera(true);
    }
}

void SoftCamera::setImageSrcPathAsync(const QString &val, const QString &old, const bool &trusted)
{
    Q_UNUSED(old);
    Q_UNUSED(trusted);

    QDir imageDir(val);

    m_imageList.clear();
    imageDir.setNameFilters(QStringList() << "*.bmp" << "*.jpg");
    QDirIterator it(imageDir);
    while (it.hasNext()) {
        it.next();
        m_imageList.append(it.filePath());
    }
    m_imageReadIndex = 0;

    if (isSpotCamera()) {
        ImageSrcDev::s_spotDetectimageCount += m_imageList.size();
        qInfo() << QString("%1: imagelist=%2, totalCount=%3").arg(name()).arg(m_imageList.size()).arg(ImageSrcDev::s_spotDetectimageCount);
        if (m_imageList.isEmpty()) {
            ImageSrcDev::s_spotDetectWaitFinishCount--;
            qInfo() << QString("%1 spot finished").arg(name());
        }
    }
}

void SoftCamera::setIntervalAsync(const int val, const int old)
{
    if (val != old) {
        if (!m_bStartStage) {
            m_timer->setInterval(val);
        }
    }
}

void SoftCamera::onTimeout()
{
    if (m_bStartStage)
    {
        m_bStartStage = false;
        m_timer->setInterval(m_imageInterval.value().toInt());
//        qInfo() << QString("Timer starStage! cameraId=%1 Now=%2").arg(id()).arg(QDateTime::currentMSecsSinceEpoch());
        return;
    }

    if (!m_imageList.empty()) {
        if (m_imageReadIndex >= m_imageList.size()) {
            if (isSpotCamera()) {
                m_imageList.clear();
                m_imageReadIndex = 0;
                ImageSrcDev::s_spotDetectWaitFinishCount--;
                qInfo() << QString("%1 spot finished").arg(name());
                return;
            }

            if (m_isLoop.value().toBool()) {
                m_imageReadIndex = 0;
            } else {
                return;
            }
        }

        QImage image(m_imageList.at(m_imageReadIndex));
        if (!image.isNull()) {
            switch (image.format()) {
            case QImage::Format_Indexed8:
                if (image.isGrayscale()) {
                    image = image.convertToFormat(QImage::Format_Grayscale8);
                } else {
                    image = image.convertToFormat(QImage::Format_BGR888);
                }
                break;
            case QImage::Format_RGB32:
                image = image.convertToFormat(QImage::Format_BGR888);
                break;
            default:
                break;
            }
            
            QFileInfo path(m_imageList.at(m_imageReadIndex));
            QString fileName = path.baseName();
            sendImageData(image, QDateTime::currentMSecsSinceEpoch(), fileName.toLongLong());
        }

        m_imageReadIndex++;
    }
}
