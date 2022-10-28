#include "DahuaCamera.h"
#include "IMVApi.h"
#include "../AssistLib/Tea.h"
#include <QDateTime>
#include "BaseLib/EngineProject.h"
#include <QDebug>

DahuaCamera::DahuaCamera(int id, EngineProject *project) :
    ImageSrcDev(id, project),
    m_Imv_Handle(NULL),
    m_findCameraTimer(new QTimer(this))
{
    m_gain.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool trusted) {
        return onGainRawChange(newValue.toDouble(), oldValue.toDouble(), trusted);
    });

    m_exposeTime.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool trusted) {
        return onExposeTimeChange(newValue.toDouble(), oldValue.toDouble(), trusted);
    });

    m_serialNumber.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool trusted) {
        return onSerialNumberChange(newValue.toString(), oldValue.toString(), trusted);
    });

    m_findCameraTimer->setInterval(5000);
    m_findCameraTimer->setSingleShot(true);
    connect(m_findCameraTimer, SIGNAL(timeout()), SLOT(onFindTimerTimeout()));
}

void DahuaCamera::onStarted()
{
    bool bCameraStartGrab = cameraStartGrab();
    if (bCameraStartGrab)
    {
        qInfo().noquote() << cameraInfoStr() << "StartGrab is success.";
    }
}

void DahuaCamera::onStopped()
{
    bool bCameraStopGrab = cameraStopGrab();
    if (bCameraStopGrab)
    {
        qInfo().noquote() << cameraInfoStr() << "StopGrab is success.";
    }
}

void DahuaCamera::cleanup()
{
    QMetaObject::invokeMethod(m_findCameraTimer, "deleteLater", Qt::BlockingQueuedConnection);

    cameraStopGrab();
    cameraCloseConnect();

    ImageSrcDev::cleanup();
}

void DahuaCamera::offlineCleanup()
{
    cameraStopGrab();
    cameraCloseConnect();
}

// 图像捕获回调函数
static void onFrameCallback(IMV_Frame* pFrame, void* pUser)
{
    DahuaCamera *pDahuaCamera = (DahuaCamera*)pUser;
    if (NULL == pDahuaCamera)
    {
        qInfo("pDahuaCamera is NULL!");
        return;
    }

    qint64 generate_time = QDateTime::currentMSecsSinceEpoch();
    QImage my_image;

    if ((int)pFrame->frameInfo.pixelFormat == IMV_EPixelType::gvspPixelMono8)
    {
        my_image = QImage((uchar *)pFrame->pData, (int)pFrame->frameInfo.width, (int)pFrame->frameInfo.height, QImage::Format_Grayscale8).copy();
    }
    else
    {
        IMV_PixelConvertParam stPixelConvertParam;

        //构建图像转换过程。将相机的原始图像，转换为QImage
        my_image = QImage(pFrame->frameInfo.width, pFrame->frameInfo.height, QImage::Format_BGR888);

        stPixelConvertParam.nWidth = pFrame->frameInfo.width;
        stPixelConvertParam.nHeight = pFrame->frameInfo.height;
        stPixelConvertParam.ePixelFormat = pFrame->frameInfo.pixelFormat;
        stPixelConvertParam.pSrcData = pFrame->pData;
        stPixelConvertParam.nSrcDataLen = pFrame->frameInfo.size;
        stPixelConvertParam.nPaddingX = pFrame->frameInfo.paddingX;
        stPixelConvertParam.nPaddingY = pFrame->frameInfo.paddingY;
        stPixelConvertParam.eBayerDemosaic = demosaicNearestNeighbor;
        stPixelConvertParam.eDstPixelFormat = gvspPixelBGR8;
        stPixelConvertParam.pDstBuf = my_image.bits();
        stPixelConvertParam.nDstBufSize = my_image.sizeInBytes();

        int ret = IMV_PixelConvert(pDahuaCamera->getIMV_Handle(), &stPixelConvertParam);
        if (IMV_OK != ret)
        {
            qInfo("IMV_PixelConvert is failed.");
            return;
        }
    }
    pDahuaCamera->sendImageData(my_image, generate_time);
}

// 相机连接事件通知回调函数
static void onReconnectCallback(const IMV_SConnectArg* pConnectArg, void* pUser)
{
    DahuaCamera *pDahuaCamera = (DahuaCamera *)pUser;

    if (NULL == pDahuaCamera)
    {
        qInfo("pDahuaCamera is NULL!");
        return;
    }

    // 离线通知
    if (offLine == pConnectArg->event)
    {
        pDahuaCamera->offlineCleanup();
        pDahuaCamera->setOnline(false);

        QMetaObject::invokeMethod(pDahuaCamera->m_findCameraTimer, "start");
    }
}

bool DahuaCamera::findAndInitCamera(const QString &serialNumber)
{
    unsigned int cameraIndex;
    int ret;

    // 发现的相机列表
    IMV_DeviceList deviceInfoList;
    ret = IMV_EnumDevices(&deviceInfoList, interfaceTypeGige);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << QString("Dahua: Enumeration devices failed! ErrorCode[%1]").arg(ret);
        return false;
    }

    //查找指定的相机
    for (cameraIndex = 0; cameraIndex < deviceInfoList.nDevNum; cameraIndex++)
    {
        if (deviceInfoList.pDevInfo[cameraIndex].serialNumber == serialNumber)
        {
            qDebug() << "Find dahua camera :(" << serialNumber << ")";
            break;
        }
    }

    if (cameraIndex == deviceInfoList.nDevNum)
    {
        qInfo() << "can not find dahua camera:(" << serialNumber << ")";
        return false;
    }

    //找到相机，创建句柄
    ret = IMV_CreateHandle(&m_Imv_Handle, modeByIndex, (void*)&cameraIndex);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << ":Create devHandle failed!";
        return false;
    }

    if (!cameraConnect())
    {
        IMV_DestroyHandle(m_Imv_Handle);
        m_Imv_Handle = NULL;
        return false;
    }

    // 设备连接状态事件回调函数
    ret = IMV_SubscribeConnectArg(m_Imv_Handle, onReconnectCallback, this);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << ":Create connect-callback function failed!";
    }

    setTrggerMode();
    onGainRawChange(m_gain.value().toDouble(), m_gain.value().toDouble(), true);
    onExposeTimeChange(m_exposeTime.value().toDouble(), m_exposeTime.value().toDouble(), true);
    if (m_enabled.value().toBool() != false)
    {
        cameraStartGrab();
    }
    setOnline(true);

    return true;
}

void DahuaCamera::onFindTimerTimeout()
{
    bool result = findAndInitCamera(m_serialNumber.value().toString());
    if (!result)
    {
        QMetaObject::invokeMethod(m_findCameraTimer, "start");
    }
}

bool DahuaCamera::cameraConnect()
{
    int ret;

    if (NULL == m_Imv_Handle)
    {
        qDebug("Dahua camera connect fail, no camera.");
        return false;
    }
    
    if (IMV_IsOpen(m_Imv_Handle))
    {
        qDebug("DaHua camera is already connected.");
        return true;
    }

    // 打开相机
    ret = IMV_Open(m_Imv_Handle);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "connect failed." << QString("ErrorCode[%1]").arg(ret);
        return false;
    }

    // 设置触发器
    ret = IMV_SetEnumFeatureSymbol(m_Imv_Handle, "TriggerSelector", "FrameStart");
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "setTriggerSelector failed." << QString("ErrorCode[%1]").arg(ret);
    }

    qDebug("Dahua camera connect success.");
    return true;
}

bool DahuaCamera::cameraCloseConnect()
{
    int ret;

    if (NULL == m_Imv_Handle)
    {
        qDebug("Dahua camera disconnect fail, no camera.");
        return false;
    }
    
    if (!IMV_IsOpen(m_Imv_Handle))
    {
        qDebug("DaHua camera is already disconnected.");
        return true;
    }

    // 关闭相机
    ret = IMV_Close(m_Imv_Handle);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "disconnect failed." << QString("ErrorCode[%1]").arg(ret);
    }

    ret = IMV_DestroyHandle(m_Imv_Handle);
    if (IMV_OK != ret)
    {
        qDebug().noquote() << cameraInfoStr() << "destroy failed." << QString("ErrorCode[%1]").arg(ret);
    }

    m_Imv_Handle = NULL;

    qDebug("Dahua camera disconnected success.");
    return true;
}

bool DahuaCamera::cameraStartGrab()
{
    IMV_DeviceInfo devInfo;
    int ret;

    if (NULL == m_Imv_Handle)
    {
        return false;
    }

    memset(&devInfo, 0, sizeof(devInfo));
    ret = IMV_GetDeviceInfo(m_Imv_Handle, &devInfo);
    if (IMV_OK != ret)
    {
        qInfo() << QString("Get device info failed! ErrorCode[%1]").arg(ret);
        return false;
    }

    if (!verifyEncryptCode(devInfo.serialNumber, m_accessCode.value().toString()))
    {
        qInfo() << "Dahua camera(" << m_serialNumber.value().toString() << "), access code is not matched.";
        return false;
    }

    if (IMV_IsGrabbing(m_Imv_Handle))
    {
        qDebug() << "Dahua camera is already start grebbing.";
        return true;
    }

    ret = IMV_AttachGrabbing(m_Imv_Handle, onFrameCallback, this);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "attach GrabCallBack failed." << QString("ErrorCode[%1]").arg(ret);
        return false;
    }

    //使能相机开始捕获图像
    ret = IMV_StartGrabbing(m_Imv_Handle);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "start grabbing failed." << QString("ErrorCode[%1]").arg(ret);
        return false;
    }
    
    return true;
}

bool DahuaCamera::cameraStopGrab()
{
    int ret;

    if (NULL == m_Imv_Handle)
    {
        return false;
    }
    
    if (!IMV_IsGrabbing(m_Imv_Handle))
    {
        qDebug("Dahua camera is already stop grabbing");
        return false;
    }

    ret = IMV_StopGrabbing(m_Imv_Handle);
    if (IMV_OK != ret)
    {
        qInfo().noquote() << cameraInfoStr() << "stop grabbing failed." << QString("ErrorCode[%1]").arg(ret);
        return false;
    }

    return true;
}

bool DahuaCamera::onSerialNumberChange(const QString &val, const QString &old, const bool &trusted)
{
    if (val != old)
    {
        //停止相机发现定时器
        QMetaObject::invokeMethod(m_findCameraTimer, "stop");

        if (m_Imv_Handle != NULL)
        {
            cameraStopGrab();
            cameraCloseConnect();
        }

        bool result = findAndInitCamera(val);
        if (!result)
        {
            //启动阶段未找到相机，启动定时器寻找对应的相机
            //非启动阶段，找不到相机不再启用定时器，系列号设置失败。
            if (trusted)
            {
                QMetaObject::invokeMethod(m_findCameraTimer, "start");
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}

bool DahuaCamera::onGainRawChange(const double &val, const double &old, const bool &trusted)
{
    int ret;

    Q_UNUSED(old)
    if (m_Imv_Handle == NULL)
    {
        if (trusted)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (!IMV_IsOpen(m_Imv_Handle))
        {
            qDebug("Dahua camera, get GainRaw node failed.");
            return false;
        }

        ret = IMV_SetDoubleFeatureValue(m_Imv_Handle, "GainRaw", val);
        if (IMV_OK != ret)
        {
            double min, max;
            IMV_GetDoubleFeatureMin(m_Imv_Handle, "GainRaw", &min);
            IMV_GetDoubleFeatureMax(m_Imv_Handle, "GainRaw", &max);
            qInfo().noquote() << QString("Dahua camera setGainRaw failed. the value must in [%1, %2]").arg(min).arg(max);
            return false;
        }

        double temp_val;
        IMV_GetDoubleFeatureValue(m_Imv_Handle, "GainRaw", &temp_val);
        qDebug() << "Dahua camera, new gain set success, the new value is " << temp_val;
        return true;
    }
}

bool DahuaCamera::onExposeTimeChange(const double &val, const double &old, const bool &trusted)
{
    int ret;

    Q_UNUSED(old)
    if (m_Imv_Handle == NULL)
    {
        if (trusted)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (!IMV_IsOpen(m_Imv_Handle))
        {
            qDebug("Dahua camera, get ExposureTime node failed.");
            return false;
        }

        ret = IMV_SetDoubleFeatureValue(m_Imv_Handle, "ExposureTime", val);
        if (IMV_OK != ret)
        {
            double min, max;
            IMV_GetDoubleFeatureMin(m_Imv_Handle, "ExposureTime", &min);
            IMV_GetDoubleFeatureMax(m_Imv_Handle, "ExposureTime", &max);
            qInfo().noquote() << QString("Dahua camera setExposureTime failed. the value must in [%1, %2]").arg(min).arg(max);
            return false;
        }

        double temp_val;
        IMV_GetDoubleFeatureValue(m_Imv_Handle, "ExposureTime", &temp_val);
        qDebug() << "Dahua camera, new ExposureTime set success, the new value is " << temp_val;
        return true;
    }
}

bool DahuaCamera::setTrggerMode()
{
    int ret;

    if (m_Imv_Handle == NULL)
    {
        return false;
    }
    else
    {
        if ((project()->productType() == PRODUCT_TYPE_BOX) && (id() == 1)) {
            // 设置相机周期性拍照，帧速率上限设置为 15fps
            ret = IMV_SetBoolFeatureValue(m_Imv_Handle, "AcquisitionFrameRateEnable", true);
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setAcquisitionFrameRateEnable[=true] failed. ErrorCode[%1]").arg(ret);
                return false;
            }
            ret = IMV_SetDoubleFeatureValue(m_Imv_Handle, "AcquisitionFrameRate", 1.0);
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setAcquisitionFrameRate[=15.0] failed. ErrorCode[%1]").arg(ret);
                return false;
            }
            ret = IMV_SetEnumFeatureSymbol(m_Imv_Handle, "AcquisitionMode", "Continuous");
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setAcquisitionMode Continuous failed. ErrorCode[%1]").arg(ret);
                return false;
            }
            // 设置触发模式
            ret = IMV_SetEnumFeatureSymbol(m_Imv_Handle, "TriggerMode", "Off");
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setTriggerMode[=Off] failed. ErrorCode[%1]").arg(ret);
                return false;
            }
        } else {
            // 设置外部信号触发相机拍照
            ret = IMV_SetEnumFeatureSymbol(m_Imv_Handle, "TriggerMode", "On");
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setTriggerMode[=On] failed. ErrorCode[%1]").arg(ret);
                return false;
            }
            // 设置触发源
            ret = IMV_SetEnumFeatureSymbol(m_Imv_Handle, "TriggerSource", "Line1");
            if (IMV_OK != ret)
            {
                qInfo().noquote() << QString("Dahua camera setTriggerSource[=Line1] failed. ErrorCode[%1]").arg(ret);
                return false;
            }
        }

        qDebug() << "Dahua camera, setTriggerMode succ!";
    }
    return true;
}
