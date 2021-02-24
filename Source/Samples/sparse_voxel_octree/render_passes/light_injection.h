#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "voxel_meta.slangh"

class light_injection : public BaseGraphicsPass, public std::enable_shared_from_this<light_injection> {
private:
    light_injection(const Scene::SharedPtr& pScene, const Program::Desc& injectionProgDesc, Program::DefineList& programDefines);
    voxel_meta voxelMeta_ = {};

public:
    using SharedPtr = std::shared_ptr<light_injection>;
    virtual ~light_injection() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui_render(Gui::Group& group);

    void on_inject_light(RenderContext* pContext, const Texture::SharedPtr& pShadowmap, const Texture::SharedPtr& pAlbedoTexture, const Texture::SharedPtr& pNormalTexture);
    void on_down_sampler(RenderContext* pContext);

    void set_voxel_meta(const voxel_meta& meta) { voxelMeta_ = meta; }
};
