#include "PatchCore.h"
#include <QtDebug>

PatchCore::PatchCore() : m_patchcore_pt(nullptr)
{
}

PatchCore::~PatchCore()
{
}

bool PatchCore::init(Config_Data &config)
{
    try
    {
        m_patchcore_pt = new PatchCorePt(config, 1);
    }
    catch (std::exception ex)
    {
        qInfo() << "patchcore init error !";
        return false;
    }

    return true;
}

bool PatchCore::update(Config_Data &config)
{
    if (m_patchcore_pt != nullptr)
    {
        return m_patchcore_pt->update(config);
    }

    return false;
}

void PatchCore::cleanup()
{
    if (m_patchcore_pt != nullptr)
    {
        delete m_patchcore_pt;
        m_patchcore_pt = nullptr;
    }
}

PatchCore_Result PatchCore::infer(const std::vector<cv::Mat> &images)
{
    if (m_patchcore_pt != nullptr)
    {
        return m_patchcore_pt->infer(images);
    }
    else
    {
        PatchCore_Result patchCoreResult;

        patchCoreResult.result = false;

        return patchCoreResult;
    }
}
