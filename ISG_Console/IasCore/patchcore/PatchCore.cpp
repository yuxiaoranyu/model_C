#include "PatchCore.h"

PatchCore::PatchCore()
    : m_patchcore_pt(nullptr)
{
}

PatchCore::~PatchCore()
{
}

bool PatchCore::init(Config_Data &config)
{
    m_patchcore_pt = new PatchCorePt(config, 1);

    return true;
}

void PatchCore::cleanup()
{
    if (m_patchcore_pt != nullptr)
    {
        delete m_patchcore_pt;
        m_patchcore_pt = nullptr;
    }
}

RoiInfo PatchCore::infer(const std::vector<cv::Mat> &images)
{
    RoiInfo results = m_patchcore_pt->infer(images);

    return results;
}
