#include "AICtrl.h"
#include <QDebug>

AICtrl::AICtrl(int id, EngineProject *project) :
    EngineObject(id, project),
    m_channelID(id)
{
    m_imageGroup = new ImageGroup(id, project, this);
    m_batchInfer = new BatchInfer(id, project, this);
}

AICtrl::~AICtrl()
{
    delete m_imageGroup;
    delete m_batchInfer;
}

bool AICtrl::init()
{
    m_imageGroup->init();
    m_batchInfer->init();
    return true;
}

void AICtrl::cleanup()
{
    onStopped();
    m_imageGroup->cleanup();
    m_batchInfer->cleanup();
}

void AICtrl::onStarted()
{
}

void AICtrl::onStopped()
{
}
