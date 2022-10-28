#ifndef IMAGESRCDEV_H
#define IMAGESRCDEV_H

#include <QAtomicInt>
#include <QThread>
#include "BaseLib/EngineObject.h"
#include "Multipler/ImageMultipler.h"

#define IMAGE_SAVE_NONE 0     //不保存任何图像
#define IMAGE_SAVE_ALL 1      //保存全部错误和全部正确图像

#define MAX_CAMERA_ID 32 //最大的相机ID，必须与图像综合允许的相机ID范围保持一致

class ImageSrcDev : public EngineObject
{
    Q_OBJECT

public:
    ImageSrcDev(int id, EngineProject *project);
    ~ImageSrcDev();

    bool init() override;           // 系统启动，读取配置文件后，启动图像源线程
    void cleanup() override;        // 启停函数
    void onStarted() override;      // 启停函数
    void onStopped() override;      // 启停函数

    static const QList<ImageSrcDev *> &devices();  //维护系统的所有相机
    static const QList<int> getCameraIdList();     //获取相机的ID列表，不包括点检相机
    static int onlineCameraCount() { return s_onlineCameraCount; }   // 在线相机数量
    static int realOnlineCameraCount() { return (s_onlineCameraCount - s_virtualCameraCount); }   // 在线相机数量，不包括点检相机
    static int getRealOnlineCameraCount(int min, int max);  // 获取一个区间内的真实相机的在线数，包括min和max

    void setOnline(bool online);
    void setSoftCamera(bool isSoftCamera);
    void setSpotCamera(bool isSpotCamera);
    bool isOnline() const { return m_online; }
    bool isSoftCamera() const { return m_isSoftCamera; }
    bool isSpotCamera() const { return m_isSpotCamera; }
    void sendImageData(const QImage &image, uint64_t imageGenerateTime, uint64_t fileNameId = 0); //相机图像入队列发送接口
               //fileNameId: IOP使用图像的ImageId作为文件名称，点检功能中，IOP要求设备返回图像文件原始的ImageId

    static int s_virtualCameraCount;          //点检相机数
    static int s_spotDetectimageCount;        // 等待点检的图像总数
    static int s_spotDetectWaitFinishCount;   // 等待点检检测结束的相机数量

protected:
    Attribute m_serialNumber;       // 相机系列号
    Attribute m_accessCode;         // 相机接入码
    Attribute m_gain;               // 相机增益
    Attribute m_exposeTime;         // 相机曝光时间

    virtual bool onSerialNumberChange(const QString &, const QString &, const bool &trusted);    // 序列号修改
    virtual bool onGainRawChange(const double &, const double &, const bool &trusted);           // 增益修改
    virtual bool onExposeTimeChange(const double &, const double &, const bool &trusted);        // 曝光时间修改
    virtual bool setTrggerMode();      // 触发模式

signals:
    void imageDataSignal(const IMAGE_DATA &imageData);

private slots:
    void sendImageDataAsync(QImage image, uint64_t imageGenerateTime, uint64_t fileNameId);	//图像处理槽
    void onSaveImage(const QImage &image, uint64_t imageTime);  //图像保存槽
    void onStatisticTimeout();		//定时器槽
	void onBoxShapeSignal(); 	//箱缺条产品，箱成形槽

private:
    bool saveStrategyCheck();                       // 根据图像检测结果和保存策略，确定是否进行图像存储
    QStringList makeImageName(uint64_t imageTime);  // 构建图像文件路径

    static QAtomicInt           s_onlineCameraCount;
    static QList<ImageSrcDev *> s_imageSrcDevices;  // 图像源列表

private:
    QThread   m_thread;
    QTimer    *m_statisticTimer;
    volatile  bool m_online;        // 是否在线
    bool      m_isSoftCamera;       // 是否SoftTimer相机
    bool      m_isSpotCamera;       // 是否是点检相机
    QImage    m_Image;              //箱缺条产品，记录最近的箱成形图片

    //图像检测器相关的参数
    Attribute m_imageSaveStrategy;  // 图像检测器的图像保存策略
    Attribute m_imageSaveRatio;     // 图像检测器保存图像的保存比例
    Attribute m_imageSaveFormat;    // 图像检测器的保存的图像格式
    Attribute m_captureCompensation;    // IO单板拍照比ISG实际捕获时间要早，向IO单板发送图像检测时间时，加上预设定的补偿值

    int       m_detectTotalCount;       // 检测的图像总数
    int       m_imageSaveRatioIndex;    // 正确图像按比例保存时，当前图像处在的位置
    int       m_imageReportIndex;       // 上报频率控制变量
    int       m_imageCountRecordCnt;    // 检测的图像数周期性记入日志，周期控制变量
};

#endif // IMAGESRCDEV_H
