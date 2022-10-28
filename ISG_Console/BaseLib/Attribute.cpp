#include "Attribute.h"
#include <QDebug>
#include "EngineObject.h"

Attribute::Attribute(const QString &key, const QJsonValue &value, EngineObject *object, const bool save, const QString product) :
    m_object(object),
    m_key(key),
    m_value(value),
    m_save(save),
    m_product(product)
{
    Q_ASSERT(object);
    object->registerAttribute(*this); //将Attribute加入到父节点的链表中；
}

void Attribute::defineGetter(Attribute::Getter &&getter)
{
    m_getter = std::move(getter); //m_getter()的返回值定义为true时，表示需要对Attribute的m_value重新赋值
}

void Attribute::defineSetter(Attribute::Setter &&setter)
{
    m_setter = std::move(setter); //m_setter()的返回值定义为false时，表示入参不合法
}

QJsonValue Attribute::value() const
{
    QJsonValue current;
    {
        QMutexLocker lock(object()->mutex());
        current = m_value;
    }
    if (m_getter && m_getter(current))  //current值可能会被自定义的函数修改掉
    {
        QMutexLocker lock(object()->mutex());
        m_value = current;
    }
    return current;
}

bool Attribute::setValue(const QJsonValue &value, const bool trusted)
{
    QJsonValue current;
    {
        QMutexLocker lock(object()->mutex());
        current = m_value;
    }
    if (value.type() != current.type())
    {
        qInfo().noquote() << EngineObject::tr("Attribute: type mismatch %1.%2 =")
                                 .arg(object()->name())
                                 .arg(m_key)
                          << value;
        return false;
    }
    if (m_setter && !m_setter(value, current, trusted))
    {
        qInfo().noquote() << EngineObject::tr("Attribute: failed to set %1.%2 =")
                                 .arg(object()->name())
                                 .arg(m_key)
                          << value;
        return false;
    }
    {
        QMutexLocker lock(object()->mutex());
        m_value = value;
    }
    return true;
}
