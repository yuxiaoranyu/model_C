#ifndef IASJSONPARSER_H
#define IASJSONPARSER_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

class IasJsonParser
{
public:
    explicit IasJsonParser(const QString& file_path);

public:
    QJsonValue get(const QString& keys);  //获取json键值(可以采用"key1/key2"的方式获取)
    static int getMethod(const QJsonObject &inputJson); //获取json对象中的method值
    static int getId(const QJsonObject &inputJson); //获取json对象中的id值
    static int getComponentType(const QJsonObject &inputJson);  //获取json对象中的ComponentType值
    static int getComponentId(const QJsonObject &inputJson);  //获取json对象中的componentId值
    static int getGroup(const QJsonObject &inputJson); //获取json对象中的分组值，龙岩条包有左右通道特性，分成两组;
    static int getFeature(const QJsonObject &inputJson); //获取json对象中的特征值，龙岩条包有左右通道特性，有特征匹配检测对象;
    static int getLocateIdX(const QJsonObject &inputJson); //获取json对象中的locator_x_id值
    static int getLocateIdY(const QJsonObject &inputJson); //获取json对象中的locator_y_id值
    static QString toQString(const QJsonObject &inputJson); //json对象转换成QString

private:
    bool load(); //加载算法脚本文件
//    bool set(const QString& keys, QJsonValue value); //设置json键值(可以采用"key1/key2"的方式设置)
    bool isNumber(QString str); //判断字符串是不是纯数字

private:
    QString m_filePath;      // Json脚本文件全路径
    bool    m_loadSuccess;   // Json脚本文件是否load成功
    QJsonDocument m_jsonDoc; // Json脚本文件对应的QJsonDocument
};

#endif // IASJSONPARSER_H
