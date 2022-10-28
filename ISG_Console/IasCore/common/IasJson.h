#ifndef IASJSON_H
#define IASJSON_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

class IasJson
{
public:
    explicit IasJson(const std::string &param);

public:
    QJsonValue get(const QString& keys);  //获取json键值(采用"key1/key2"的方式获取)

private:
    QJsonDocument m_jsonDoc;
    bool m_loadSuccess;
};

#endif // IASJSON_H
