#include "YoloResNetInference.h"
#include "../common/IasCommonDefine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>

YoloResnetAiInferenceFactory yoloResnetFactory;

void YoloResnetAiInit()
{
    RegisterBaseAiInferenceFactory(YOLO_RESNET_COMPONENT_TYPE, &yoloResnetFactory);
    qDebug() << "yoloresnet register!";
}

YoloResnetAiInference::YoloResnetAiInference()
{
}

YoloResnetAiInference::~YoloResnetAiInference()
{
    if (yoloparam.yolo_model != nullptr)
    {
        delete yoloparam.yolo_model;
        yoloparam.yolo_model = nullptr;
    }

    if (resnetparam.resnet_model != nullptr)
    {
        delete resnetparam.resnet_model;
        resnetparam.resnet_model = nullptr;
    }
}

bool YoloResnetAiInference::load(const std::string &param)
{
    std::string aiPath = getPath(); //获取算法路径
    QString jsonQString = QString::fromStdString(param);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonQString.toLocal8Bit());
    QJsonObject jsonDocObj = jsonDoc.object();
    QJsonObject module_param = jsonDocObj.value("module_param").toObject();

    if (module_param.size() != 2)
    {
        qInfo() << "module_param.size() != 2";
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
            yoloparam.model_path = aiPath + "/resource/" + "1.engine";
            yoloparam.yolo_model = new Yolov5();
            yoloparam.yolo_model->init(yoloparam.model_path);
        }
        else
        {
            //初始化模型
            resnetparam.model_path = aiPath + "/resource/" + "2.engine";
            resnetparam.resnet_model = new ResNet();
            resnetparam.resnet_model->init(resnetparam.model_path);

            //加载标签文件
            QFile file(QString::fromStdString(aiPath + "/" + "2.lbl"));
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
        return false;
    }

    return true;
}

//构建坐标转换矩阵
bool YoloResnetAiInference::transformCoordinates()
{
    //读取坐标转换配置文件
    std::string calib_path = getPath() + "/../config/calib";
    QString calib_config = QString::fromStdString(calib_path + "/calib.json");
    QFile file(calib_config);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull())
    {
        return false;
    }
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    QJsonObject obj = doc.object();
    if (obj.isEmpty())
    {
        return false;
    }
    std::cout << "obj size ***************" << obj.size() << std::endl;

    QJsonObject calibObj = obj.value("calib").toObject();
    for (int index = 0; index < calibObj.size(); index++)
    {
        QString channelId = QString::fromStdString("channel_" + std::to_string(index + 1));
        QJsonObject channelObj = calibObj.value(channelId).toObject();

        QJsonArray origin_point_Array = channelObj.value("origin_point").toArray();
        QJsonArray board_size_Array = channelObj.value("board_size").toArray();
        QJsonArray square_size_Array = channelObj.value("square_size").toArray();
        cv::Point origin_point_;  // 棋盘格左上角的像素点
        cv::Size board_size_;     // 标定板上每行、列的角点数
        cv::Size square_size_ ;   // 实际测量得到的标定板上每个棋盘格的大小

        origin_point_.x = origin_point_Array[0].toInt();
        origin_point_.y = origin_point_Array[1].toInt();
        board_size_.width = board_size_Array[0].toInt();
        board_size_.height = board_size_Array[1].toInt();
        square_size_.width = square_size_Array[0].toInt();
        square_size_.height = square_size_Array[1].toInt();

        cv::Mat image_input = cv::imread(calib_path + "/" + std::to_string(index + 1) + ".bmp"); //棋盘图(标定板)
        if (image_input.empty())
        {
            qInfo()<<"read transformCoordinates images failed,check image is in path";
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
        m_transform_map[index + 1] = cv::findHomography(image_points_buf, object_points);
        std::cout << "tf_map size" << index + 1 << "\t" << m_transform_map[index + 1].size() << std::endl;
    }
    return true;
}

bool YoloResnetAiInference::update(const std::string &param)
{
    (void)param;

    return true;
}

std::vector<BaseAIResult> YoloResnetAiInference::infer(const std::vector<int> &ids, const std::vector<cv::Mat> &images)
{
    BaseAIResult inferResult;
    std::vector<BaseAIResult> inferResultVector;

    if (ids.size() > 1) {
        qInfo() << "YoloResnetAiInference error, only support one image infer, input image size is: " << ids.size();
        for (size_t i = 0; i < ids.size(); i++)
        {
            inferResult.setResult(false);
            inferResultVector.push_back(inferResult);
        }
        return inferResultVector;
    }

    std::vector<std::vector<YoloResult>> yolo_result_data;
    std::vector<ResNetResult> resnet_result_data;
    QJsonObject isgObj;
    QJsonArray drawArray, pos_infoArray;
    bool result = true;

    yolo_result_data = yoloparam.yolo_model->infer(images);
    std::vector<cv::Mat> resnet_input;
    for (size_t yolo_index = 0; yolo_index < yolo_result_data.size(); yolo_index++)
    {
        std::cout << "yolo result size " << yolo_result_data[yolo_index].size() << std::endl;
        if (yolo_result_data[yolo_index].size() != 0)
        {
            int x1 = yolo_result_data[yolo_index][0].box.tl().x;
            int y1 = yolo_result_data[yolo_index][0].box.tl().y;
            int x2 = yolo_result_data[yolo_index][0].box.br().x;
            int y2 = yolo_result_data[yolo_index][0].box.br().y;
            std::cout << "******************x1******************** " << x1 << std::endl;
            std::cout << "******************x2******************** " << x1 << std::endl;
            std::cout << "******************y1******************** " << y1 << std::endl;
            std::cout << "******************y2******************** " << y2 << std::endl;
            cv::Rect crop(cv::Point(std::max(x1, 0), std::max(y1, 0)),
                          cv::Point(std::min(x2, images[0].cols), std::min(y2, images[0].rows)));
            resnet_input.push_back(images[yolo_index](crop));
            resnet_result_data = resnetparam.resnet_model->infer(resnet_input);
            int classId = resnet_result_data[yolo_index].classId;

            bool isDefective = resnetparam.label_array[classId].toObject()["isDefective"].toBool();
            //if (isDefective==true)
            if (1)
            {
                result=false;
                std::vector<cv::Point2f> in_pos;
                in_pos.push_back(cv::Point2f(x1 + x2 / 2, y1 + y2 / 2));
                std::vector<cv::Point2f> out_pos;
                std::cout << "size map " << m_transform_map[ids[0]].size() << std::endl;
                cv::perspectiveTransform(in_pos, out_pos, m_transform_map[ids[0]]);

                QJsonObject labelObj, pos_infoObj, paramObj;
                QJsonArray pointArray;
                QJsonObject ltObj, rbObj, kickObj; //左上角坐标,右上角坐标

                pos_infoObj.insert("id", int(yolo_index));
                pos_infoObj.insert("moduleId", 1);
                pos_infoObj.insert("classes", classId);
                pos_infoObj.insert("label", "it is a label !");
                ltObj.insert("x", x1);
                ltObj.insert("y", y1);
                rbObj.insert("x", x2);
                rbObj.insert("y", y2);
                pointArray.append(ltObj);
                pointArray.append(rbObj);
                labelObj.insert("points", pointArray);
                labelObj.insert("color", "08A7FA");
                labelObj.insert("shapeType", 2);
                drawArray.append(labelObj);
                pos_infoObj.insert("draw", drawArray);
                pos_infoObj.insert("result", 1);
                pos_infoObj.insert("params", paramObj);
                pos_infoArray.append(pos_infoObj);
                kickObj.insert("x", (int)out_pos[0].x);
                kickObj.insert("y", (int)out_pos[0].y);
                isgObj.insert("kick_coordinate", kickObj);
            }

        }
    }
    inferResult.setResult(result);

    QJsonObject ResultObj;

    ResultObj.insert("componentId", getId());
    ResultObj.insert("componentType", YOLO_RESNET_COMPONENT_TYPE);
    ResultObj.insert("pos_info", pos_infoArray);
    ResultObj.insert("isg_info", isgObj);
    ResultObj.insert("result", 1);
    QString pos_infoSrting = QJsonDocument(ResultObj).toJson();
    inferResult.setPositionInfo(pos_infoSrting.toStdString());

    QString isgInfoString = QJsonDocument(isgObj).toJson();

    inferResult.setIsgInfo(isgInfoString.toStdString());

    for (size_t i = 0; i < ids.size(); ++i)
    {
        inferResultVector.push_back(inferResult);
    }
    std::cout << "infer result ***************************** " << inferResult.getPositionInfo() << std::endl;
    return inferResultVector;
}

bool YoloResnetAiInference::remove()
{
    if (yoloparam.yolo_model != nullptr)
    {
        delete yoloparam.yolo_model;
        yoloparam.yolo_model = nullptr;
    }

    if (resnetparam.resnet_model != nullptr)
    {
        delete resnetparam.resnet_model;
        resnetparam.resnet_model = nullptr;
    }

    return true;
}
