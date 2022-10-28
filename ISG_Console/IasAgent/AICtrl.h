#ifndef AICTRL_H
#define AICTRL_H

#include <QObject>
#include "BaseLib/EngineObject.h"
#include "imageGroup.h"
#include "batchInfer.h"

class AICtrl : public EngineObject
{
    Q_OBJECT

public:
    AICtrl(int id, EngineProject *project);
    ~AICtrl();
    virtual bool init() override;
    //重写基类的虚函数
    void onStarted() override; //启停函数
    void onStopped() override; //启停函数
    void cleanup() override;

    int getChannelId() { return m_channelID;}  //获取通道id
    ImageGroup *getImageGroup() {return m_imageGroup;}
    BatchInfer *getBatchInfer() {return m_batchInfer;}

private:
    int m_channelID; // 通道id
    ImageGroup *m_imageGroup;
    BatchInfer *m_batchInfer;
};

#endif // AICTRL_H
