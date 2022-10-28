#include "BaslerCCDCamera.h"
#include "../AssistLib/Tea.h"
#include "BaseLib/EngineProject.h"
#include <QDebug>
#include <QDateTime>
#include <QMutexLocker>

//定时器在自定义的线程运行
//相机回调函数在相机线程运行
//json参数修改在TCP线程运行
BaslerCCDCamera::BaslerCCDCamera(int id, EngineProject *project) :
    ImageSrcDev(id, project),
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
    m_FrameStartTimestamp(0),
#endif
    m_pInstantCamera(0),
    m_findCameraTimer(new QTimer(this)),
    m_heartbeatTimer(new QTimer(this)),
    m_ReconnectCameraTimer(new QTimer(this))
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

    m_ReconnectCameraTimer->setInterval(2000);
    m_ReconnectCameraTimer->setSingleShot(true);
    connect(m_ReconnectCameraTimer, SIGNAL(timeout()), SLOT(onReconnectTimerTimeout()));

    m_heartbeatTimer->setInterval(2000);
    connect(m_heartbeatTimer, SIGNAL(timeout()), SLOT(onHeartbeatTimerTimeout()));
}

void BaslerCCDCamera::onStarted()
{
    bool bCameraStartGrab = cameraStartGrab();
    if (bCameraStartGrab)
    {
        qInfo("BaslerCCD camera StartGrab is success.");
    }
}

void BaslerCCDCamera::onStopped()
{
    bool bCameraStopGrab = cameraStopGrab();
    if (bCameraStopGrab)
    {
        qInfo("BaslerCCD camera StopGrab is success.");
    }
}

void BaslerCCDCamera::cleanup()
{
    QMetaObject::invokeMethod(m_findCameraTimer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_ReconnectCameraTimer, "deleteLater", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_heartbeatTimer, "deleteLater", Qt::BlockingQueuedConnection);

    cameraStopGrab();
    if (m_pInstantCamera)
    {
        m_pInstantCamera->DestroyDevice();
        delete m_pInstantCamera;
        m_pInstantCamera = NULL;
    }

    ImageSrcDev::cleanup();
}

bool BaslerCCDCamera::findAndInitCamera(const QString &serialNumber)
{
    //获取相机列表
    CTlFactory &TlFactory = CTlFactory::GetInstance();
    DeviceInfoList_t lstDevices;
    size_t i;

    TlFactory.EnumerateDevices(lstDevices);
    for (i = 0; i < lstDevices.size(); i++)
    {
        if (lstDevices[i].GetSerialNumber() == serialNumber)
        {
            qDebug() << "found Basler Camera" << m_name.value().toString()<< " (" << serialNumber <<")";
            break;
        }
    }
    if (i == lstDevices.size())
    {
        qInfo() << "can not find basler camera :" << m_name.value().toString()<< " (" << serialNumber <<")";
        return false;
    }

    IPylonDevice *pPylonDevice = TlFactory.CreateDevice(lstDevices[i]);
    if (!pPylonDevice)
    {
        qDebug() << "Basler Camera CreateDevice failed.";
        return false;
    }
    try
    {
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
        m_pInstantCamera = new CBaslerUniversalInstantCamera(pPylonDevice);
#endif
#if (defined USING_CAMERA_BASLER)
        m_pInstantCamera = new CInstantCamera(pPylonDevice);
#endif
        //获取设备信息，以便断线重连
        m_DevInfo.SetDeviceClass(m_pInstantCamera->GetDeviceInfo().GetDeviceClass());
        m_DevInfo.SetSerialNumber(m_pInstantCamera->GetDeviceInfo().GetSerialNumber());

        // 安装标准的软触发事件处理器，以使能软触发
        //m_pCamera->RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);

        //安装配置事件处理器
        m_pInstantCamera->RegisterConfiguration(this, RegistrationMode_Append, Cleanup_None);

        // 安装图像事件处理器，在这里对图像流进行发送
        m_pInstantCamera->RegisterImageEventHandler(this, RegistrationMode_Append, Cleanup_None);

//事件注册
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
        m_pInstantCamera->RegisterCameraEventHandler( this, "FrameStartEventData", eMyFrameStartEvent, RegistrationMode_Append, Cleanup_None );
//        m_pInstantCamera->RegisterCameraEventHandler( this, "ExposureEndEventData", eMyExposureEndEvent, RegistrationMode_Append, Cleanup_None );
        m_pInstantCamera->GrabCameraEvents = true;
#endif
//事件注册

        m_pInstantCamera->Open();
        setTrggerMode();
        onGainRawChange(m_gain.value().toDouble(), m_gain.value().toDouble(), true);
        onExposeTimeChange(m_exposeTime.value().toDouble(), m_exposeTime.value().toDouble(), true);

        INodeMap &NodeMap = m_pInstantCamera->GetNodeMap();
        gcstring strPixelFormat = CEnumerationPtr(NodeMap.GetNode("PixelFormat"))->ToString();

        if (strPixelFormat.find("Mono") != (size_t)-1)
        { //如果是黑白图，一律转为PIXEL_FORMAT_MONO8
            m_ImgConverter.OutputPixelFormat = PixelType_Mono8;
            m_imageFormat = QImage::Format_Grayscale8;
        }
        else
        { //其他都是彩色图，一律转为PIXEL_FORMAT_RGB24
            m_ImgConverter.OutputPixelFormat = PixelType_BGR8packed;
            m_imageFormat = QImage::Format_BGR888;
        }

//事件注册
#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
        if (m_pInstantCamera->EventSelector.TrySetValue( EventSelector_FrameStart ))
        {   // Enable it.
            if (!m_pInstantCamera->EventNotification.TrySetValue( EventNotification_On ))
            {
                m_pInstantCamera->EventNotification.SetValue( EventNotification_GenICamEvent );
            }
        }
/*
        if (m_pInstantCamera->EventSelector.TrySetValue( EventSelector_ExposureEnd ))
        {   // Enable it.
            if (!m_pInstantCamera->EventNotification.TrySetValue( EventNotification_On ))
            {
                m_pInstantCamera->EventNotification.SetValue( EventNotification_GenICamEvent );
            }
        }
*/
#endif
//事件注册

        QMetaObject::invokeMethod(m_heartbeatTimer, "start");
        if(m_enabled.value().toBool())
        {
            cameraStartGrab();
        }

        setOnline(true);
        return true;
    }
    catch (GenICam::GenericException &e)
    {
        qInfo() << "BaslerCCD camera init failed. Reason:" << e.GetDescription();
        if (m_pInstantCamera)
        {
            m_pInstantCamera->DestroyDevice();
            delete m_pInstantCamera;
            m_pInstantCamera = NULL;
        }
        return false;
    }
}

void BaslerCCDCamera::onFindTimerTimeout()
{
    bool result = findAndInitCamera(m_serialNumber.value().toString());
    if (!result)
    {
        QMetaObject::invokeMethod(m_findCameraTimer, "start");
    }
}

#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
void BaslerCCDCamera::OnCameraEvent( CBaslerUniversalInstantCamera& camera, intptr_t userProvidedId, GenApi::INode* pNode )
{
    Q_UNUSED(pNode)

    switch (userProvidedId)
    {
        case eMyFrameStartEvent:
            m_FrameStartTimestamp =   camera.FrameStartEventTimestamp.GetValue();
//            qDebug() << "camera " << id() << "Frame start event " << " FrameStartEventTimestamp: " << camera.FrameStartEventTimestamp.GetValue() << " getTimestamp(): "  << getTimestamp()<< " currentMSecsSinceEpoch(): " << QDateTime::currentMSecsSinceEpoch();
            break;

//        case eMyExposureEndEvent:
//            qDebug() << "camera " << id() << "Frame ExposureEnd event " << " ExposureEndFrameId: " << camera.ExposureEndEventFrameID.GetValue() << "ExposureEndTimestamp: " << camera.ExposureEndEventTimestamp.GetValue() << " getTimestamp(): " << getTimestamp()   << " currentMSecsSinceEpoch(): " << QDateTime::currentMSecsSinceEpoch();
//            break;

        default:
            qDebug() << "===========OnCameraEvent, userProvidedId is " << userProvidedId;
            break;
    }
}
#endif

void BaslerCCDCamera::onHeartbeatTimerTimeout()
{
    try
    {
        INodeMap &NodeMap = m_pInstantCamera->GetNodeMap();
        CIntegerPtr(NodeMap.GetNode("GainRaw"))->GetValue();
    } catch (const GenericException &e)
    {
        // The camera device has been removed. This caused the exception.
        qInfo() <<"onHeartbeatTimerTimeout, reason: " << e.GetDescription() << endl;
        setOnline(false);
        QMetaObject::invokeMethod(m_heartbeatTimer,"stop");
        QMetaObject::invokeMethod(m_ReconnectCameraTimer, "start");
    }
}

void BaslerCCDCamera::onReconnectTimerTimeout()
{
    //reconnect Camera
    try
    {
        if (m_pInstantCamera)
        {
            m_pInstantCamera->DestroyDevice();
        }
        DeviceInfoList_t filter;
        DeviceInfoList_t devices;
        CTlFactory &tlFactory = CTlFactory::GetInstance();

        filter.push_back(m_DevInfo);
        if (tlFactory.EnumerateDevices(devices, filter) > 0)
        {
            // The camera has been found. Create and attach it to the Instant Camera object.
            m_pInstantCamera->Attach(tlFactory.CreateDevice(devices[0]));
        }

        if (m_pInstantCamera->IsPylonDeviceAttached())
        {
            m_pInstantCamera->Open();
            //重新恢复所有的参数
            setTrggerMode();
            onGainRawChange(m_gain.value().toDouble(), m_gain.value().toDouble(), true);
            onExposeTimeChange(m_exposeTime.value().toDouble(), m_exposeTime.value().toDouble(), true);
            QMetaObject::invokeMethod(m_heartbeatTimer, "start");
            if(m_enabled.value().toBool())
            {
                cameraStartGrab();
            }

            setOnline(true);
            qInfo() << "device reconnect success";
            return ;
        }
    }
    catch (GenICam::GenericException &e)
    {
        qInfo() << "BaslerCCD camera reconnect failed. Reason:" << e.GetDescription();
    }
    QMetaObject::invokeMethod(m_ReconnectCameraTimer, "start");
}

bool BaslerCCDCamera::setTrggerMode()
{
    if (m_pInstantCamera == NULL)
    {
        return false;
    }
    else
    {
        try
        {
            INodeMap &NodeMap = m_pInstantCamera->GetNodeMap();

            if ((project()->productType() == PRODUCT_TYPE_BOX) && (id() == 1)) {
                // 关闭触发器的外部触发模式
                CEnumerationPtr(NodeMap.GetNode("TriggerMode"))->FromString("Off");
                // 将相机帧速率上限设置为 15fps
                CBooleanPtr(NodeMap.GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
                CFloatPtr(NodeMap.GetNode("AcquisitionFrameRate"))->SetValue(15.0);
            } else {
                // 启用触发器的外部触发模式
                CEnumerationPtr(NodeMap.GetNode("TriggerMode"))->FromString("On");
                // 将帧开始触发器的触发源设置为Line1
                CEnumerationPtr(NodeMap.GetNode("TriggerSource"))->FromString("Line1");
            }
        }
        catch (GenICam::GenericException &e)
        {
            qInfo() << "BaslerCCD camera setTrggerMode failed. Reason:" << e.GetDescription();
            return false;
        }
    }
    return true;
}

bool BaslerCCDCamera::cameraStartGrab()
{
    if (m_pInstantCamera && m_pInstantCamera->IsPylonDeviceAttached())
    {
        //相机准入控制
        if (!verifyEncryptCode(m_pInstantCamera->GetDeviceInfo().GetSerialNumber(), m_accessCode.value().toString()))
        {
            qInfo() << "BaslerCCD camera, access code is not matched.";
            return false;
        }

        try
        {
            m_pInstantCamera->StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
        }
        catch (GenICam::GenericException &e)
        {
            qInfo() << "BaslerCCD camera StartGrabbing failed. Reason:" << e.GetDescription();
            return false;
        }
        return true;
    }
    else
    {
        qInfo() << "BaslerCCD camera StartGrabbing failed, m_pCamera is not attached.";
        return false;
    }
}

bool BaslerCCDCamera::cameraStopGrab()
{
    if (m_pInstantCamera && m_pInstantCamera->IsPylonDeviceAttached())
    {
        try
        {
            m_pInstantCamera->StopGrabbing();
        }
        catch (GenICam::GenericException &e)
        {
            qInfo() << "BaslerCCD camera StopGrabbing failed. Reason:" << e.GetDescription();
            return false;
        }
        return true;
    }
    else
    {
        qInfo() << "BaslerCCD camera StopGrabbing failed, m_pCamera is not attached.";
        return false;
    }
}

#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
int64_t BaslerCCDCamera::getTimestamp()
{
    try
    {
        if (m_pInstantCamera)
        {
            m_pInstantCamera->GevTimestampControlLatch.Execute();
            return m_pInstantCamera->GevTimestampValue.GetValue();
        }

    } catch (const GenericException &e)
    {
        qInfo() <<"getTimestamp fail, reason: " << e.GetDescription() << endl;
    }

    return 0;
}
#endif

void BaslerCCDCamera::OnImageGrabbed(CInstantCamera &camera, const CGrabResultPtr &ptrGrabResult)
{
    QImage my_image;
    Q_UNUSED(camera);
    if (ptrGrabResult->GrabSucceeded())
    {
        qint64 generate_time = QDateTime::currentMSecsSinceEpoch();

#if ((defined USING_CAMERA_BASLER_6_0) || (defined USING_CAMERA_BASLER_6_2))
        int64_t grabbedTimestamp = getTimestamp();

        if (grabbedTimestamp > m_FrameStartTimestamp)
        {
            int64_t timestampGap = (grabbedTimestamp - m_FrameStartTimestamp) * 8; // 1 tick = 8 ns

            if (timestampGap<1000000000) //异常保护，图像捕获回调需在Framestart发生的1s内
            {
                timestampGap = timestampGap /1000000;
                generate_time -= timestampGap;
                qDebug() << "camera " << id()  << " OnImageGrabbed, time adjust: " << timestampGap;
            }
        }
#endif

//        qDebug() << "camera " << id()  << "OnImageGrabbed, getTimestamp(): " << getTimestamp()  << " currentMSecsSinceEpoch(): " << QDateTime::currentMSecsSinceEpoch();

        if (m_ImgConverter.ImageHasDestinationFormat(ptrGrabResult))
        {
            my_image = QImage((uint8_t *)ptrGrabResult->GetBuffer(),
                              ptrGrabResult->GetWidth(),
                              ptrGrabResult->GetHeight(),
                              m_imageFormat).copy();
        }
        else
        {
            CPylonImage TargetImage;
            m_ImgConverter.Convert(TargetImage, ptrGrabResult);
            my_image = QImage((uint8_t *)TargetImage.GetBuffer(),
                              ptrGrabResult->GetWidth(),
                              ptrGrabResult->GetHeight(),
                              m_imageFormat == QImage::Format_BGR888?ptrGrabResult->GetWidth() * 3 : ptrGrabResult->GetWidth(),
                              m_imageFormat).copy();
        }

        sendImageData(my_image, generate_time);
    }
    else
    {
        qInfo() << "Basler Grab Error:" << ptrGrabResult->GetErrorDescription().c_str();
    }
}

void BaslerCCDCamera::OnGrabError(CInstantCamera &camera, const char *errorMessage)
{
    Q_UNUSED(camera);
    qInfo() << name() << " OnGrabError, reason: " << errorMessage;
}

void BaslerCCDCamera::OnImagesSkipped(CInstantCamera &camera, size_t countOfSkippedImages)
{
    Q_UNUSED(camera);
    qInfo() << name() << " OnImagesSkipped, losted frame number: " << countOfSkippedImages;
}

void BaslerCCDCamera::OnCameraDeviceRemoved(CInstantCamera &camera)
{
    Q_UNUSED(camera);
    qInfo() << "basler camara is offline, start timer to reconnect...";
    QMetaObject::invokeMethod(m_ReconnectCameraTimer, "start");

    setOnline(false);
}

bool BaslerCCDCamera::onSerialNumberChange(const QString &val, const QString &old, const bool &trusted)
{
    if (val != old)
    {
        //停止相机发现定时器
        QMetaObject::invokeMethod(m_findCameraTimer, "stop");
        QMetaObject::invokeMethod(m_ReconnectCameraTimer, "stop");

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

bool BaslerCCDCamera::onGainRawChange(const double &val, const double &old, const bool &trusted)
{
    Q_UNUSED(old);
    if (m_pInstantCamera == NULL)
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
        try
        {
            INodeMap &NodeMap = m_pInstantCamera->GetNodeMap();
            CIntegerPtr(NodeMap.GetNode("GainRaw"))->SetValue(val);
        }
        catch (GenICam::GenericException &e)
        {
            qInfo() << "BaslerCCD camera setGain failed. Reason:" << e.GetDescription();
            return false;
        }
    }
    return true;
}

bool BaslerCCDCamera::onExposeTimeChange(const double &val, const double &old, const bool &trusted)
{
    Q_UNUSED(old)
    if (m_pInstantCamera == NULL)
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
        try
        {
            INodeMap &NodeMap = m_pInstantCamera->GetNodeMap();
            CIntegerPtr(NodeMap.GetNode("ExposureTimeRaw"))->SetValue(val);
        }
        catch (GenICam::GenericException &e)
        {
            qInfo() << "BaslerCCD camera setExposeTime failed. Reason:" << e.GetDescription();
            return false;
        }
    }

    return true;
}
