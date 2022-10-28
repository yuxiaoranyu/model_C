#pragma once

#include <cstdint>

#define VERSION                 2       // 版本号 2
#define PROCESS_TYPE_ISG        0005    // 与策略中心的定义保持一致

#define IOP_LED_STATE_GREEN     0001    // 绿灯
#define IOP_LED_STATE_RED       0002    // 红灯

#define CTRL_SUCCESS            200
#define CTRL_ERROR              400

typedef enum POLICY_STORE_CMD_TYPE_ENUM
{
    PsCmd_ConfigSet                 = 1,     ///< 配置设置
    PsCmd_RegisterToPolicy          = 3,     ///< 注册
    PsCmd_CHANGE_BRANDID            = 6,     /// 切换牌号
    PsCmd_STATISTIC                 = 9,     /// 统计数据上报
    PsCmd_IasSet                    = 10,     /// 算法参数设置
    PsCmd_REBOOT                    = 1000,     /// 进程重启
    PsCmd_ACK                       = 1001,     ///< PolicyStore应答
    PsCmd_ReportBarcode             = 1005,     ///< 条形码上报
    PsCmd_Enable                    = 1100,     ///< 启动和停止运行

//告警命令字
    PsCmd_AlarmLedSet               = 3000,     ///< IOP向ISG发送告警灯状态
    PsCmd_Heartbeat                 = 3001,     ///< ISG向IOP上报心跳点
    PsCmd_ContinueKickStart         = 3002,     ///< IOP向ISG发送连踢开始、结束

//点检命令字
    PsCmd_SetWorkMode               = 3005,     ///< 切换工作模式
    PsCmd_SpotEnable                = 3006,     ///< 点检启停
    PsCmd_InquireSpotDetectStatus   = 3007,     ///< 查询点检状态(开始，终止)
    PsCmd_InquireVersionInfo        = 3008,     ///< 查询版本信息
    PsCmd_ImageReportCreat          = 8001,     ///< 创建媒体通道
    PsCmd_ImageReportMediaType      = 8002,     ///< 设置媒体通道允许传送的图片类型
    PsCmd_ImageResultReport         = 8003,     ///< ISG向IOP、IAS上报图像
    PsCmd_ImageResultReportBinning  = 8005,     ///< ISG向IOP、IAS上报图像

}PsCmdTypeEnum;

#pragma pack(1)

typedef struct tagPOLICY_INFO_MSG_HEADER_S
{
    uint64_t tranId;            ///< 操作事务号，包序号, 只支持数字。毫秒级时间戳
    uint64_t timeStamp;         ///< 操作时间戳，取值为当前时间距离1970-01-01 00:00:00的毫秒数
    uint32_t processId;         ///< 进程ID
    uint16_t processType;       ///< 进程类型：0-Policy Store,1-IAS Core，2-ISG，3-Multipler 5-三合一
    uint16_t command;           ///< 命令字
    uint16_t version;           ///< 版本号
    uint32_t len;               ///< 消息体长度
} POLICY_INFO_MSG_HEADER;

typedef struct tagIMAGE_DATA_INFO_S
{
    uint64_t objectId;          ///< 对象ID
    uint16_t cameraId;          ///< 相机标识
    uint64_t imageId;           ///< 相机的图像ID
    uint64_t genTimeStamp;      ///< 图像创建时间
    uint16_t imageType;         ///< 图像类别
    uint32_t imageWidth;        ///< 图像宽度
    uint32_t imageHeight;       ///< 图像高度
    int16_t  result;            ///< 图片结果
    uint32_t imageSize;         ///< 图像大小
    uint32_t jsonSize;          ///< json大小
} IMAGE_DATA_INFO;
#pragma pack()
