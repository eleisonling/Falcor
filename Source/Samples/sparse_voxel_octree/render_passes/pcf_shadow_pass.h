#pragma once
#include "Falcor.h"

using namespace Falcor;

class pcf_shadow_pass : public BaseGraphicsPass, public std::enable_shared_from_this<pcf_shadow_pass> {
private:
    pcf_shadow_pass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc, Program::DefineList& programDefines);
    void rebuild_shadowmap_buffers();
    void rebuild_shadow_matrix(float3 lightDir, const AABB& bounds);
    uint2 mpSize_ = uint2(4096, 4096);
    int2 pcf_kernel_size_ = { 4, 4 };
    Scene::SharedPtr mpScene_ = nullptr;
    Fbo::SharedPtr mpShadowMap_ = nullptr;
    bool smChanged_ = false;
    float3 cachedMainLightDir_ = {};

    FullScreenPass::SharedPtr mpApplyPass_ = nullptr;
    float4x4 lightViewProj_ = {};
    float4x4 shadowMatrix_ = {};

    Sampler::SharedPtr mpPCFSampler_ = nullptr;
    
public:
    using SharedPtr = std::shared_ptr<pcf_shadow_pass>;
    virtual ~pcf_shadow_pass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui_render(Gui::Group& group);

    void generate_shadowmap(RenderContext* pContext);
    void deferred_apply(RenderContext* pContext, const Fbo::SharedPtr& pSceneFbo, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr mpTexSampler);

    bool refresh_rebuild();
    Texture::SharedPtr get_shadowmap() const { return mpShadowMap_->getDepthStencilTexture(); }
    const float4x4& get_shadow_matrix() const { return shadowMatrix_; }
};
