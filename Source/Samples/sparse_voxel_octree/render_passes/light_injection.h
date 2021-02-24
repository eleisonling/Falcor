#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "voxel_meta.slangh"

class light_injection : public std::enable_shared_from_this<light_injection> {
private:
    light_injection(const Scene::SharedPtr& pScene, Program::DefineList& programDefines);
    void create_light_injection_shaders(Program::DefineList& programDefines);
    void create_light_injection_resources();
    voxel_meta voxelMeta_ = {};
    Scene::SharedPtr mpScene_ = nullptr;
    ComputePass::SharedPtr mpInjection_ = nullptr;
    ComputePass::SharedPtr mpDownSample_ = nullptr;
    Texture::SharedPtr mpRadius_ = nullptr;

public:
    using SharedPtr = std::shared_ptr<light_injection>;
    virtual ~light_injection();

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui_render(Gui::Group& group);

    void on_inject_light(RenderContext* pContext, const Texture::SharedPtr& pShadowmap, const float4x4& shadowMatrix, const Texture::SharedPtr& pAlbedoTexture, const Texture::SharedPtr& pNormalTexture, const svo_meta& meta);
    void on_down_sampler(RenderContext* pContext);

    void set_voxel_meta(const voxel_meta& meta) { voxelMeta_ = meta; }
};
