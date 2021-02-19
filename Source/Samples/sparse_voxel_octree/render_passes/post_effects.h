#pragma once
#include "Falcor.h"

using namespace Falcor;

class post_effects : public std::enable_shared_from_this<post_effects> {
    post_effects(const Program::DefineList& programDefines);
    Fbo::SharedPtr mpPingpongBuffer_[2] = {};
    uint32_t curIndx_ = 0;

    // bloom
    float expExposure_ = -1.5f;
    const float initialMinLog_ = -12.0f;
    const float initialMaxLog_ = 4.0f;
    float bloomThreshold_ = 0.03f;
    float upSampleBlendFactor_ = 0.4f;
    float bloomStrength_ = 0.25f;
    float expMinExposure_ = -8.0f;
    float expMaxExposure_ = 8.0f;
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
    Sampler::SharedPtr mpUpBlurSample_ = nullptr;

    // tone map
    ComputePass::SharedPtr mpToneMap_ = nullptr;

    // present
    FullScreenPass::SharedPtr mpPresent_ = nullptr;

    void create_bloom_resource(const Program::DefineList& programDefines);
    void rebuild_bloom_buffers(uint32_t width, uint32_t height);

    void create_tonemap_resource(const Program::DefineList& programDefines);
    void create_present_resource(const Program::DefineList& programDefines);

    void do_bloom(RenderContext* pContext, const Sampler::SharedPtr& texSampler);
    void do_bloom_up_blur(RenderContext* pContext, const Texture::SharedPtr& target, const Texture::SharedPtr& highSource, const Texture::SharedPtr& lowSource);
    void do_tone_map(RenderContext* pContext, const Sampler::SharedPtr& texSampler);
    void do_present(RenderContext* pContext, const Sampler::SharedPtr& texSampler, const Fbo::SharedPtr& pDestFbo);

public:
    using SharedPtr = std::shared_ptr<post_effects>;
    virtual ~post_effects();

    inline Fbo::SharedPtr get_fbo() const { return mpPingpongBuffer_[curIndx_]; }
    void on_swapchain_resize(uint32_t width, uint32_t height);
    static SharedPtr create(const Program::DefineList& programDefines = Program::DefineList());
    void on_gui_render(Gui::Group& group);
    void on_execute(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& texSampler);
    void do_clear(RenderContext* pRenderContext);
};
