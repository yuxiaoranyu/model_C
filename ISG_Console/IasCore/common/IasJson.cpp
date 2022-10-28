#include "IasJson.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

IasJson::IasJson(const std::string &param) :
    m_loadSuccess(false)
{
    QString jsonQString = QString::fromStdString(param);

    m_jsonDoc = QJsonDocument::fromJson(jsonQString.toLocal8Bit());
    if (!m_jsonDoc.isEmpty()) {
        m_loadSuccess = true;
    }
}

QJsonValue IasJson::get(const QString& keys)
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
            int  keyIndex;  //将数字串转换成数字作为数组下标
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
