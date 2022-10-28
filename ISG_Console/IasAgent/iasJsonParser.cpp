#include "iasJsonParser.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>

IasJsonParser::IasJsonParser(const QString& file_path):
    m_filePath(file_path),
    m_loadSuccess(false)
{
    if (load()) {
        m_loadSuccess = true;
    }
}

bool IasJsonParser::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo().noquote() << "could't open: " + m_filePath;
        return false;
    }
    QByteArray all_data = file.readAll();
    file.close();

    QJsonParseError json_error;
    m_jsonDoc = QJsonDocument::fromJson(all_data, &json_error);
    if (json_error.error != QJsonParseError::NoError) {
        qInfo().noquote() << "open json error!";
        return false;
    }

    if (m_jsonDoc.isEmpty()) {
        qInfo().noquote() << "json file empty!";
        return false;
    }

    return true;
}

QJsonValue IasJsonParser::get(const QString& keys)
{
    QStringList keys_list = keys.split("/", QString::SkipEmptyParts);
    QString key;
    QJsonValue json_val;

    if (!m_loadSuccess) {
        return json_val;
    }

    if (m_jsonDoc.isObject()) {
        json_val = m_jsonDoc.object();
    } else {
        json_val = m_jsonDoc.array();
    }

    for (int i = 0; i < keys_list.size(); ++i) {
        key = keys_list.at(i);
        if (json_val.isObject()) {
            json_val = json_val.toObject().value(key);
        } else if (json_val.isArray()) {
            int  keyIndex;
            bool bInt;

            keyIndex = key.toInt(&bInt);
            if (!bInt) {
                qInfo().noquote() << "get json error --> " << keys;
                return QJsonValue();
            }
            json_val = json_val.toArray().at(keyIndex);
        } else {
            qInfo().noquote() << "get json error --> " << keys;
            return QJsonValue();
        }
    }

    return json_val;
}

/*
bool IasJsonParser::set(const QString& keys, QJsonValue value)
{
    QStringList keys_list = keys.split("/", QString::SkipEmptyParts);
    QJsonValue json_val;
    QVector<QJsonValue> json_vec;

    if (!m_parseSuccess) {
        return false;
    }

    if (m_configType == QJsonValue::Object) {
        json_vec.push_back(m_jsonDoc.object());
        json_val = m_jsonDoc.object().value(keys_list.at(0));
    } else {
        json_vec.push_back(m_jsonDoc.array());
        if (isNumber(keys_list.at(0)))
            json_val = m_jsonDoc.array().at(keys_list.at(0).toInt());
        else {
            qInfo().noquote() << "set json error";
            return false;
        }
    }

    for (int i = 1; i < keys_list.size(); ++i) {
        json_vec.push_back(json_val);
        if (json_val.isObject()) {
            json_val = json_val.toObject().value(keys_list.at(i));
        } else if (json_val.isArray()) {
            if (isNumber(keys_list.at(i)))
                json_val = json_val.toArray().at(keys_list.at(i).toInt());
            else {
                qInfo().noquote() << "set json error";
                return false;
            }
        } else {
            break;
        }
    }

    QJsonValue val = value;

    for (int j = json_vec.size() - 1; j >= 0; --j) {
        if (json_vec[j].isObject()) {
            QJsonObject obj = json_vec[j].toObject();
            obj[keys_list.at(j)] = val;
            val = obj;
        } else if (json_vec[j].isArray()) {
            if (isNumber(keys_list.at(j))) {
                QJsonArray array = json_vec[j].toArray();
                array[keys_list.at(j).toInt()] = val;
                val = array;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    if (m_configType == QJsonValue::Object)
        m_jsonDoc.setObject(val.toObject());
    else if (m_configType == QJsonValue::Array)
        m_jsonDoc.setArray(val.toArray());

    return true;
}
*/

bool IasJsonParser::isNumber(QString str)
{
    QByteArray ba = str.toLatin1();//QString 转换为 char*
    const char *s = ba.data();

    while (*s && *s >= '0' && *s <= '9') {
        s++;
    }
    if (*s) {
        //不是纯数字
        return false;
    } else {
        //纯数字
        return true;
    }
}

int IasJsonParser::getMethod(const QJsonObject &inputJson)
{
    return inputJson.value("method").toInt();
}

int IasJsonParser::getId(const QJsonObject &inputJson)
{
    return inputJson.value("id").toInt();
}

int IasJsonParser::getComponentType(const QJsonObject &inputJson)
{
    return inputJson.value("componentType").toInt();
}

int IasJsonParser::getComponentId(const QJsonObject &inputJson)
{
    return inputJson.value("componentId").toInt();
}

int IasJsonParser::getGroup(const QJsonObject &inputJson)
{
    return inputJson.value("group").toInt(1);  //默认为组1
}

int IasJsonParser::getFeature(const QJsonObject &inputJson)
{
    return inputJson.value("is_feature").toInt(2); //默认为非特征
}

int IasJsonParser::getLocateIdX(const QJsonObject &inputJson)
{
    return inputJson.value("locator_x_id").toInt();
}

int IasJsonParser::getLocateIdY(const QJsonObject &inputJson)
{
    return inputJson.value("locator_y_id").toInt();
}

QString IasJsonParser::toQString(const QJsonObject &inputJson)
{
    return QJsonDocument(inputJson).toJson();
}
