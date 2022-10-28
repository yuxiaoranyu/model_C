#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <QJsonValue>
#include <functional>
#include <utility>

class EngineObject;

class Attribute
{
public:
    //提供自定义Getter函数能力，定义Getter函数样式。自定义函数返回值定义为true时，表示自定义函数可能会修改m_value的值，需要将修改后的值赋回m_value。
    typedef std::function<bool(QJsonValue &)> Getter;

    //提供自定义Setter函数能力，定义Setter函数样式。自定义函数返回值定义为false时，表示入参不合法
    typedef std::function<bool(const QJsonValue &newValue, const QJsonValue &oldValue, bool trusted)> Setter;

public:
    Attribute(const QString &key, const QJsonValue &value, EngineObject *object, const bool save = true, const QString product = "all");

public:
    void defineGetter(Getter &&getter); //使用这个函数，定义Attribute的Getter函数
    void defineSetter(Setter &&setter); //使用这个函数，定义Attribute的Setter函数
    EngineObject *object() const { return m_object; }
    const QString &key() const { return m_key; }
    QJsonValue value() const;
    bool saveToParams() const { return m_save; }
    QString getProduct() const { return m_product; }

    //部分参数涉及硬件操作，系统启动时，硬件可能未ready，需信任配置文件定义的value，即使硬件操作失败，仍将输入value保存到m_value中；
    //硬件ready后，使用m_value，调用硬件设置接口；
    //非启动阶段，不信任输入值，硬件设置失败，不更新m_value；
    bool setValue(const QJsonValue &value, const bool trusted = false);

private:
    EngineObject *const m_object; //父Object
    const QString m_key;   //键
    mutable QJsonValue m_value; //值
    const bool m_save;  //enable等Attribute，不需要保存配置文件; 定义Attribute是否保存到product_params.json文件
    QString m_product;  //Attribute适用的产品
    Getter m_getter;    //自定义的Getter函数
    Setter m_setter;    //自定义的Setter函数
};

#endif // ATTRIBUTE_H
