#pragma once
#include "Falcor.h"

using namespace Falcor;

class pcf_shadow_pass : public BaseGraphicsPass, public std::enable_shared_from_this<pcf_shadow_pass> {
    pcf_shadow_pass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc, const Program::Desc& applyProgDesc, Program::DefineList& programDefines);
    void rebuild_shadowmap_buffers();
    uint2 mpSize_ = uint2(2048, 2048);
    float2 pcf_kernel_size_ = { 2.0f, 2.0f };
    Scene::SharedPtr mpScene_ = nullptr;
    Fbo::SharedPtr mpShadowMap_ = nullptr;
    Fbo::SharedPtr mpSceneFbo_ = nullptr;
    bool smChanged_ = false;
    float3 cachedMainLightDir_ = {};

    FullScreenPass::SharedPtr mpApplyPass_ = nullptr;
    float4x4 lightViewProj_ = {};

public:
    using SharedPtr = std::shared_ptr<pcf_shadow_pass>;
    virtual ~pcf_shadow_pass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui_render(Gui::Group& group);

    void generate_shadowmap(RenderContext* pContext);
    void deferred_apply(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);

    bool refresh_rebuild();
};
