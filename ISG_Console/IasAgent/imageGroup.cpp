#include "imageGroup.h"
#include "../BaseLib/EngineProject.h"
#include "../ImgSrc/ImageSrcDev.h"
#include <QDateTime>
#include <QDebug>

ImageGroup::ImageGroup(int id, EngineProject *project, AICtrl *pAICtrl) :
    m_project(project),
    m_AICtrl(pAICtrl),
    m_timer(new QTimer(this)),
    m_channelID(id),
    m_Batch(true)
{
    m_timer->setInterval(10); //烟包成组超时。如果间隔设置过大，成组开销将被拉大
    connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
}

void ImageGroup::cleanup()
{
    QMetaObject::invokeMethod(m_timer, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

bool ImageGroup::init()
{
    //订阅相机的图像数据信号
    for (auto &devices : ImageSrcDev::devices()) {
        if (m_cameraIdList.contains(devices->id())) {
            connect(devices, SIGNAL(imageDataSignal(const IMAGE_DATA &)), this, SLOT(onImageData(const IMAGE_DATA &)));
        }
    }

    m_timer->start();

    moveToThread(&m_thread);
    m_thread.setObjectName(QString("ImageGroup_%1").arg(m_channelID));
    m_thread.start();
    return true;
}

void ImageGroup::onImageData(const IMAGE_DATA &imageData)
{
    int cameraCount = ImageSrcDev::getRealOnlineCameraCount(m_cameraIdList.first(), m_cameraIdList.last());
    QHash<uint64_t, IMAGE_GROUP_DATA>::iterator imageGroupIterator = m_groupHash.find(imageData.objectId);

    if (imageGroupIterator != m_groupHash.end())
    {
        imageGroupIterator.value().cameraIdVector.push_back(imageData.cameraId);
        imageGroupIterator.value().imageIdVector.push_back(imageData.imageId);
        imageGroupIterator.value().imageDataVector.push_back(imageData.imageDataCV);

        if ((m_project->getRunType() == RunType_Infer) &&
            ((int)imageGroupIterator.value().imageIdVector.size() >= cameraCount))
        {
            Q_EMIT imageGroupSignal(imageGroupIterator.value());
            m_groupHash.remove(imageData.objectId);
        }
        return;
    }

    IMAGE_GROUP_DATA imageGroupData;
    imageGroupData.objectId = imageData.objectId;
    imageGroupData.cameraIdVector.push_back(imageData.cameraId);
    imageGroupData.imageIdVector.push_back(imageData.imageId);
    imageGroupData.imageDataVector.push_back(imageData.imageDataCV);
    imageGroupData.genTimeStamp = QDateTime::currentMSecsSinceEpoch();

    if (!m_Batch || (cameraCount <= 1) || (m_project->getRunType() == RunType_Spot)) {
        Q_EMIT imageGroupSignal(imageGroupData);
    } else {
         m_groupHash.insert(imageData.objectId, imageGroupData);
    }
}

void ImageGroup::onTimeout()
{
    qint64 nowTime = QDateTime::currentMSecsSinceEpoch();

    for (auto item = m_groupHash.begin(); item != m_groupHash.end();) {
        //如果烟包的创建时间超过了烟包各图片的最大间隔，回收烟包结构
        if (nowTime - item.value().genTimeStamp >= m_project->imageMaxInterval()) {
            Q_EMIT imageGroupSignal(item.value());
            item = m_groupHash.erase(item);
        } else {
            item++;
        }
    }
}
