#ifndef ENGINEOBJECT_H
#define ENGINEOBJECT_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QVector>
#include <memory>
#include "Attribute.h"

class EngineProject;

typedef std::shared_ptr<EngineObject> ObjectPtr;

//相机、检测代理、执行机构涉及从配置文件获取参数，EngineObject作为这些对象的基类
class EngineObject : public QObject
{
    Q_OBJECT

public:
    explicit EngineObject(EngineProject *project) : EngineObject(0, project) {}
    EngineObject(int id, EngineProject *project);
    virtual bool init();   // 初始化函数

public:
    QJsonValue getAttr(const QString &key) const;                                       //根据Attribute的key字符串，返回Attribute的值
    bool setAttr(const QString &key, const QJsonValue &value, const bool trusted = false); //根据Attribute的key字符串，设置Attribute的值
    int id() const { return m_id; }
    void loadFromJson(const QJsonValue &json); //根据Json文件，初始化obj对象的各Attribute
    QString name() const { return m_name.value().toString(); }
    EngineProject *project() const { return m_project; }
    QJsonValue saveToJson() const; //将对象使用json格式进行保存

protected:
    virtual void cleanup();   // 关闭product_def.json前触发
    virtual void onStarted(); // 开始工作
    virtual void onStopped(); // 停止工作

private:
    friend class Attribute;
    friend class EngineProject;

    void registerAttribute(Attribute &attr); //Attribute构建时，将Attribute加入Hash和链表，供Attribute构建使用。
    QMutex *mutex() const { return &m_mutex; }

    const int m_id; // 对象ID。相机对象，使用ID作为参数，触发算法检测、进行图像和结果IOP上报
    EngineProject *const m_project;
    mutable QMutex m_mutex; // 多个线程读写对象属性时，需要加锁保护
    QHash<QString, Attribute *> m_attrMap;  //Hash保存Attribute
    QVector<Attribute *> m_attrList;   //Vector保存Attribute

protected:
    Attribute m_enabled; // 启停开关
    Attribute m_name;    // 名称，product_params.json文件定义的对象名字符串
};

#endif // ENGINEOBJECT_H
