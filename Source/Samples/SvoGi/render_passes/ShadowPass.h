#pragma once
#include "Falcor.h"

using namespace Falcor;

class ShadowPass : public BaseGraphicsPass, public std::enable_shared_from_this<ShadowPass> {
private:
    ShadowPass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc, Program::DefineList& programDefines);
    void rebuild_shadowmap_buffers();
    void rebuild_shadow_matrix(float3 lightDir, const AABB& bounds);

    int2 mDimension_ = int2(4096, 4096);
    int2 mPcfKernel_ = { 4, 4 };
    float4x4 mShadowMatrix_ = {};

    Sampler::SharedPtr mpPCFSampler_ = nullptr;
    Scene::SharedPtr mpScene_ = nullptr;
    Fbo::SharedPtr mpShadowMap_ = nullptr;

    
public:
    using SharedPtr = std::shared_ptr<ShadowPass>;
    virtual ~ShadowPass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui(Gui::Group& group);
    void on_render(RenderContext* pContext);

    Texture::SharedPtr get_shadow_map() const { return mpShadowMap_->getDepthStencilTexture(); }
    Sampler::SharedPtr get_shadow_sampler() const { return mpPCFSampler_; }
    const int2& get_pcf_kernel() const { return mPcfKernel_; }
    const float4x4& get_shadow_matrix() const { return mShadowMatrix_; }
    const int2& get_shadow_map_dimension() const { return mDimension_; }
};
