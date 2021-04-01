#pragma once
#include "Falcor.h"

using namespace Falcor;

class PostEffect : public std::enable_shared_from_this<PostEffect> {

    PostEffect(const Program::DefineList& programDefines);

    ShaderResourceView::SharedPtr mpInput_ = nullptr;
    Sampler::SharedPtr mpTexSampler_ = nullptr;
    Fbo::SharedPtr mpPingpongBuffer_[2] = {};
    uint32_t mCurIndx_ = 0;

    // bloom
    float mExpExposure_ = 1.25f;
    const float mInitialMinLog_ = -12.0f;
    const float mInitialMaxLog_ = 4.0f;
    float mBloomThreshold_ = 0.03f;
    float mUpSampleBlendFactor_ = 0.4f;
    float mBloomStrength_ = 0.25f;
    float mExpMinExposure_ = -8.0f;
    float mExpMaxExposure_ = 8.0f;
    Texture::SharedPtr mpBloomUAV1_[2] = {};
    Texture::SharedPtr mpBloomUAV2_[2] = {};
    Texture::SharedPtr mpBloomUAV3_[2] = {};
    Texture::SharedPtr mpBloomUAV4_[2] = {};
    Texture::SharedPtr mpBloomUAV5_[2] = {};
    Texture::SharedPtr mpLumaResult_ = nullptr;
    Buffer::SharedPtr mpExposure_ = nullptr;

    ComputePass::SharedPtr mpExtractAndDownsample_ = nullptr;
    ComputePass::SharedPtr mpDownSample_ = nullptr;
    ComputePass::SharedPtr mpBlur_ = nullptr;
    ComputePass::SharedPtr mpUpBlur_ = nullptr;
    Sampler::SharedPtr mpUpBlurSampler_ = nullptr;

    // tone map
    ComputePass::SharedPtr mpToneMap_ = nullptr;

    // present
    FullScreenPass::SharedPtr mpPresent_ = nullptr;

    void create_bloom_resource(const Program::DefineList& programDefines);
    void rebuild_bloom_buffers(uint32_t width, uint32_t height);

    void create_tonemap_resource(const Program::DefineList& programDefines);
    void create_present_resource(const Program::DefineList& programDefines);

    void do_clear(RenderContext* pRenderContext);
    void do_bloom(RenderContext* pContext);
    void do_bloom_up_blur(RenderContext* pContext, const Texture::SharedPtr& pTarget, const Texture::SharedPtr& pHighSource, const Texture::SharedPtr& pLowSource);
    void do_tone_map(RenderContext* pContext);
    void do_present(RenderContext* pContext, const Fbo::SharedPtr& pDestFbo);

public:
    using SharedPtr = std::shared_ptr<PostEffect>;
    virtual ~PostEffect();

    inline void set_input(const ShaderResourceView::SharedPtr& input) { mpInput_ = input; }
    inline void set_sampler(const Sampler::SharedPtr& input) { mpTexSampler_ = input; }

    void on_resize(uint32_t width, uint32_t height);
    static SharedPtr create(const Program::DefineList& programDefines = Program::DefineList());
    void on_gui(Gui::Group& group);
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& texSampler);
};
