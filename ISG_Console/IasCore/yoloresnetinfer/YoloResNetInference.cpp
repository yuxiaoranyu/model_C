#include "YoloResNetInference.h"
#include "../common/IasCommonDefine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>

YoloResnetInferenceFactory yoloResnetFactory;

void YoloResnetInferenceInit()
{
    RegisterBaseAiInferenceFactory(YOLO_RESNET_COMPONENT_TYPE, &yoloResnetFactory);
}

YoloResnetInference::YoloResnetInference()
{
}

YoloResnetInference::~YoloResnetInference()
{
    remove();
}

bool YoloResnetInference::remove()
{
    if (yoloparam.yolo_model != nullptr)
    {
        yoloparam.yolo_model->cleanup();
        delete yoloparam.yolo_model;
        yoloparam.yolo_model = nullptr;
    }

    if (resnetparam.resnet_model != nullptr)
    {
        resnetparam.resnet_model->cleanup();
        delete resnetparam.resnet_model;
        resnetparam.resnet_model = nullptr;
    }

    return true;
}

bool YoloResnetInference::load(const std::string &param)
{
    std::string aiPath = getPath(); //获取算法路径

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QString::fromStdString(param).toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();
    QJsonObject module_param = jsonDocObj.value("module_param").toObject();

    if (module_param.size() != 2)
    {
        qInfo() << "YoloResnetInference load: module_param.size() != 2";
        return false;
    }

    for (int i = 0; i < module_param.size(); i++)
    {
        QString moduleIdString = QString::fromStdString("moduleId_" + std::to_string(i));
        QJsonObject moduleJsonObject = module_param.value(moduleIdString).toObject();
        std::string moduleMethod = moduleJsonObject.value("method").toString().toStdString();
        if (moduleMethod == "yolov5")
        {
            //初始化模型
            yoloparam.model_path = aiPath + "/resource/1.engine";
            yoloparam.yolo_model = new Yolov5();
            yoloparam.yolo_model->init(yoloparam.model_path);
        }
        else
        {
            //初始化模型
            resnetparam.model_path = aiPath + "/resource/2.engine";
            resnetparam.resnet_model = new ResNet();
            resnetparam.resnet_model->init(resnetparam.model_path);

            //加载标签文件
            QFile file(QString::fromStdString(aiPath + "/resource/2.lbl"));
            if (file.open(QIODevice::ReadOnly))
            {
                QByteArray all_data = file.readAll();
                file.close();

                QJsonParseError json_error;
                QJsonDocument jsonDoc(QJsonDocument::fromJson(all_data, &json_error));
                QJsonObject rootObj = jsonDoc.object();
                resnetparam.label_array = rootObj["labels"].toArray();
            }
        }
    }

    //构建坐标转换矩阵
    if (!transformCoordinates())
    {
        qInfo() << "can not construct transform-coordinate";
        return false;
    }

    return true;
}

//构建坐标转换矩阵
bool YoloResnetInference::transformCoordinates()
{
    //读取坐标转换配置文件
    std::string calib_path = getPath() + "/../../config/calib"; //位于algorithm/config/calib目录
    QFile file(QString::fromStdString(calib_path + "/calib.json"));
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);

    if (doc.isNull())
    {
        return false;
    }

    QJsonObject obj = doc.object();
    if (obj.isEmpty())
    {
        return false;
    }

    QJsonObject calibObj = obj.value("calib").toObject();
    for (int index = 1; index < calibObj.size() + 1; index++)  //每个相机配置一个转换矩阵数据
    {
        QJsonObject channelObj = calibObj.value(QString("channel_%1").arg(index)).toObject();

        QJsonArray origin_point_Array = channelObj.value("origin_point").toArray();
        QJsonArray board_size_Array = channelObj.value("board_size").toArray();
        QJsonArray square_size_Array = channelObj.value("square_size").toArray();
        cv::Point origin_point_;  // 棋盘格左上角的像素点
        cv::Size board_size_;     // 标定板上行、列的角点数
        cv::Size square_size_ ;   // 实际测量得到的标定板上每个棋盘格的大小

        origin_point_.x = origin_point_Array[0].toInt();
        origin_point_.y = origin_point_Array[1].toInt();
        board_size_.width = board_size_Array[0].toInt();
        board_size_.height = board_size_Array[1].toInt();
        square_size_.width = square_size_Array[0].toInt();
        square_size_.height = square_size_Array[1].toInt();

        cv::Mat image_input = cv::imread(calib_path + "/" + std::to_string(index) + ".bmp"); //棋盘图(标定板)
        if (image_input.empty())
        {
            qInfo()<<"read transformCoordinates images failed!";
            return false;
        }
        std::vector<cv::Point2f> image_points_buf; // 每幅图像上检测到的角点

        // 提取角点
        if (0 == findChessboardCorners(image_input, board_size_, image_points_buf))
        {
            std::cout << "can not find chessboard corners!" << std::endl; //找不到角点
            return false;
        }

        // 角点亚像素精确化
        cv::Mat view_gray;
        cv::cvtColor(image_input, view_gray, cv::COLOR_RGB2GRAY);
        find4QuadCornerSubpix(view_gray, image_points_buf, cv::Size(5, 5)); //对粗提取的角点进行精确化

        // 构建物理坐标
        std::cout << "|    corner pos\t|    world pos\t|    image pos";
        std::vector<cv::Point2f> object_points;
        for (int i = 0; i < board_size_.height; ++i)  //标定板的行数
        {
            for (int j = 0; j < board_size_.width; ++j)  //标定板的列数
            {
                cv::Point2f realPoint;

                // 假设标定板放在世界坐标系中z=0的平面上
                realPoint.x = origin_point_.x + j * square_size_.width;
                realPoint.y = origin_point_.y + i * square_size_.height;
                object_points.push_back(realPoint);

                std::cout << std::endl;
                std::cout << "|    (" << j << ", " << i << ")\t|    ";
                std::cout << "(" << realPoint.x << ", " << realPoint.y << ")\t|    ";
                std::cout << "(" << image_points_buf[i * board_size_.width + j].x << ", " << image_points_buf[i * board_size_.width + j].y << ")";
            }
        }
        std::cout << std::endl;

        //根据图像坐标和物理坐标，构建转换矩阵
        m_transform_map[index] = cv::findHomography(image_points_buf, object_points);
        std::cout << "tf_map size" << index << "\t" << m_transform_map[index].size() << std::endl;
    }

    return true;
}

bool YoloResnetInference::update(const std::string &param)
{
    (void)param;

    return true;
}

std::vector<BaseAIResult> YoloResnetInference::infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images)
{
    BaseAIResult inferResult;
    std::vector<BaseAIResult> inferResultVector;

    if (ids.size() > 1) {
        qInfo() << "YoloResnetInference error, only support one image infer, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }

    QJsonArray pos_infoArray;
    QJsonArray kickObjArray;
    bool result = true;

    std::vector<std::vector<YoloResult>> yolo_result_data = yoloparam.yolo_model->infer(images);

    //目前batch=1，所以yolo_result_data.size固定为1
    for (size_t yolo_index = 0; yolo_index < yolo_result_data.size(); yolo_index++)
    {
//        std::cout << "yolo result size " << yolo_result_data[yolo_index].size() << std::endl;
        if (yolo_result_data[yolo_index].size() != 0)  //仅分析检测出的第一个目标
        {
            int x1 = yolo_result_data[yolo_index][0].box.tl().x;
            int y1 = yolo_result_data[yolo_index][0].box.tl().y;
            int x2 = yolo_result_data[yolo_index][0].box.br().x;
            int y2 = yolo_result_data[yolo_index][0].box.br().y;
/*
            std::cout << "******************x1******************** " << x1 << std::endl;
            std::cout << "******************x2******************** " << x1 << std::endl;
            std::cout << "******************y1******************** " << y1 << std::endl;
            std::cout << "******************y2******************** " << y2 << std::endl;
*/
            cv::Rect cropRect(cv::Point(std::max(x1, 0), std::max(y1, 0)), cv::Point(std::min(x2, images[0].cols), std::min(y2, images[0].rows)));
            std::vector<cv::Mat> resnet_input;
            resnet_input.push_back(images[yolo_index](cropRect));
            std::vector<ResNetResult> resnet_result_data = resnetparam.resnet_model->infer(resnet_input);

            int classId = resnet_result_data[yolo_index].classId; //检测目标的分类ID

            QJsonObject labelJsonObj = resnetparam.label_array[classId].toObject();
            bool isDefective = labelJsonObj["isDefective"].toBool();
            QString aliasString = labelJsonObj["alias"].toString();
            QString colorString = labelJsonObj["color"].toString("FF0000");

            if (isDefective) //杂物类别
            {
                result = false;
                std::vector<cv::Point2f> in_pos;  //杂物图像坐标
                std::vector<cv::Point2f> out_pos; //杂物物理坐标

                in_pos.push_back(cv::Point2f(x1 + x2 / 2, y1 + y2 / 2));
                cv::perspectiveTransform(in_pos, out_pos, m_transform_map[ids[0]]); //opencv透视变换

                QJsonArray pointArray;  //构建图形单元points的json
                QJsonObject ltObj, rbObj; //左上角坐标,右上角坐标

                ltObj.insert("x", x1);
                ltObj.insert("y", y1);
                rbObj.insert("x", x2);
                rbObj.insert("y", y2);
                pointArray.append(ltObj);
                pointArray.append(rbObj);

                QJsonObject drawObj; //图形单元
                QJsonArray drawArray; //图形单元数组

                drawObj.insert("points", pointArray);
                drawObj.insert("color", colorString);
                drawObj.insert("shapeType", 2);
                drawArray.append(drawObj);

                QJsonObject kickObj;  //烟丝杂物剔除坐标点json
                kickObj.insert("x", (int)out_pos[0].x);
                kickObj.insert("y", (int)out_pos[0].y);

                QJsonObject paramObj; //构建算法返回参数params的json
                paramObj.insert("kick_coordinate", kickObj);

                QJsonObject pos_infoObj;   //构建pos_info的json
                pos_infoObj.insert("id", int(yolo_index));
                pos_infoObj.insert("moduleId", 1);
                pos_infoObj.insert("classes", classId);
                pos_infoObj.insert("label", aliasString);
                pos_infoObj.insert("draw", drawArray);
                pos_infoObj.insert("result", 0);
                pos_infoObj.insert("params", paramObj);
                pos_infoArray.append(pos_infoObj);

                kickObjArray.append(kickObj);
            }
        }
    }
    inferResult.setResult(result);

    QString pos_infoSrting = QJsonDocument(pos_infoArray).toJson();
    inferResult.setPositionInfo(pos_infoSrting.toStdString());

    if (kickObjArray.size() > 0) //构建IsgInfo
    {
        QJsonObject isgObj;

        isgObj.insert("kick_coordinate", kickObjArray);
        QString isgInfoString = QJsonDocument(isgObj).toJson();
        inferResult.setIsgInfo(isgInfoString.toStdString());
    }

    for (size_t i = 0; i < ids.size(); ++i)
    {
        inferResultVector.push_back(inferResult);
    }
//    std::cout << "infer result ***************************** " << inferResult.getPositionInfo() << std::endl;
    return inferResultVector;
}
