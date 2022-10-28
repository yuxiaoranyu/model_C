#ifndef TYPE_H
#define TYPE_H

#include <QHash>
#include <QTimer>
#include <QThread>
#include <QImage>

#define AI_OPERATION_ADD                1   //加载模型
#define AI_OPERATION_DEL                2   //删除模型，目前未使用
#define AI_OPERATION_UPDATE_PARAM       3   //更新模型
#define AI_OPERATION_SAVE               100 //保存

#define DT_OPERATION_ADD                1   // 仅添加配置（定位或检测）
#define DT_OPERATION_DEL                2   // 删除配置（定位或检测）
#define DT_OPERATION_UPDATE_PARAM       3   // 更新配置参数（定位或检测）
#define DT_OPERATION_UPDATE_IMAGE       4   // 更新模板图片
#define DT_OPERATION_AUTOVERIFY         5   // 自动校核，程序自动计算所有参数（定位或检测）
#define DT_OPERATION_PROCESS            6   // 执行处理，调用定位或检测返回执行结果
#define DT_OPERATION_SAVE               100 //保存

#define DT_TYPE_LOCATOR                 1
#define DT_TYPE_DETECTOR                2

#endif // TYPE_H
