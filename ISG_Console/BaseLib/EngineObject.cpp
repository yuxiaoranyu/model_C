#include "EngineObject.h"
#include "EngineProject.h"
#include <QDebug>
#include <QJsonObject>

EngineObject::EngineObject(int id, EngineProject *project) : 
    m_id(id),
    m_project(project),
    m_enabled("enabled", false, this, false),
    m_name("name", "", this, false)
{
    Q_ASSERT(project);
    // 启停开关改变触发对象的onStarted()或onStopped()
    m_enabled.defineSetter([this](const QJsonValue &newValue, const QJsonValue &oldValue, bool) {
        bool old = oldValue.toBool();
        bool val = newValue.toBool();
        if (val != old)
        {
            if (val)
            {
                onStarted();
            }
            else
            {
                onStopped();
            }
        }
        return true;
    });
}

//系统启动时，相机等功能模块在对象创建后，根据预定义的Attribute，实现json配置的加载；
void EngineObject::loadFromJson(const QJsonValue &json)
{
    if (json.type() == QJsonValue::Object)
    {
        auto obj = json.toObject();
        // 按定义顺序设置属性
        for (Attribute *attr : m_attrList)
        {
            if (attr->saveToParams())
            {
                auto i = obj.find(attr->key());
                if (i != obj.end())
                {
                    setAttr(attr->key(), i.value(), true);
                }
            }
        }
    }
}

QJsonValue EngineObject::saveToJson() const
{
    QJsonObject obj;
    QString productType = project()->productType();

    for (Attribute *attr : m_attrList)
    {
        if (attr->saveToParams() &&
           ((attr->getProduct() == "all") || (attr->getProduct() == productType)))
        {
            obj.insert(attr->key(), attr->value());
        }
    }
    return obj;
}

void EngineObject::registerAttribute(Attribute &attr)
{
    Q_ASSERT(!m_attrMap.contains(attr.key()));
    m_attrMap.insert(attr.key(), &attr);
    m_attrList.append(&attr);
}

QJsonValue EngineObject::getAttr(const QString &key) const
{
    auto pos = m_attrMap.find(key);
    if (pos != m_attrMap.end())
    {
        return pos.value()->value();
    }
    else
    {
        qInfo().noquote() << tr("EngineObject: can not find attribute %1.%2 to get").arg(name()).arg(key);
        return QJsonValue();
    }
}

bool EngineObject::setAttr(const QString &key, const QJsonValue &value, const bool trusted)
{
    auto pos = m_attrMap.find(key);
    if (pos != m_attrMap.end())
    {
        return pos.value()->setValue(value, trusted);
    }
    else
    {
        qInfo().noquote() << tr("EngineObject: can not find attribute %1.%2 to set").arg(name()).arg(key);
        return false;
    }
}

bool EngineObject::init()
{
    return true;
}

void EngineObject::cleanup()
{
}

void EngineObject::onStarted()
{
}

void EngineObject::onStopped()
{
}
