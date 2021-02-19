#include "post_effects.h"
#include "post_effects.slangh"

namespace {
    static std::string kBloomProg = "samples/sparse_voxel_octree/render_passes/bloom.cs.slang";
    static std::string kTonemapProg = "samples/sparse_voxel_octree/render_passes/tonemap.cs.slang";
    static std::string kPresentProg = "Samples/sparse_voxel_octree/render_passes/present.ps.slang";
    static exposure_meta g_exposure = {};
}

post_effects::post_effects(const Program::DefineList& programDefines) {
    mpExposure_ = Buffer::createStructured(sizeof(exposure_meta), 1);
    g_exposure = { exposure_, 1.0f / exposure_, exposure_, 0.0f,
        initialMinLog_, initialMaxLog_, initialMaxLog_ - initialMinLog_, 1.0f / (initialMaxLog_ - initialMinLog_) };
    mpExposure_->setBlob(&g_exposure, 0, sizeof(g_exposure));

    create_bloom_resource(programDefines);
    create_tonemap_resource(programDefines);
    create_present_resource(programDefines);
}

void post_effects::create_bloom_resource(const Program::DefineList& programDefines) {
    mpExtractAndDownsample_ = ComputePass::create(kBloomProg, "extract_and_downsample", programDefines);
    mpDownSample_ = ComputePass::create(kBloomProg, "down_sample", programDefines);
    mpBlur_ = ComputePass::create(kBloomProg, "blur", programDefines);
    mpUpBlur_ = ComputePass::create(kBloomProg, "up_blur", programDefines);
    
    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point)
        .setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border)
        .setBorderColor({ 0.0f,0.0f,0.0f,0.0f });
    mpUpBlurSample_ = Sampler::create(desc);
}

void post_effects::rebuild_bloom_buffers(uint32_t width, uint32_t height) {
    mpLumaResult_ = Texture::create2D(width, height, ResourceFormat::R8Uint, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV1_[0] = Texture::create2D(width, height, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV1_[1] = Texture::create2D(width, height, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV2_[0] = Texture::create2D(width / 2, height / 2, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV2_[1] = Texture::create2D(width / 2, height / 2, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV3_[0] = Texture::create2D(width / 4, height / 4, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV3_[1] = Texture::create2D(width / 4, height / 4, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV4_[0] = Texture::create2D(width / 8, height / 8, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV4_[1] = Texture::create2D(width / 8, height / 8, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV5_[0] = Texture::create2D(width / 16, height / 16, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBloomUAV5_[1] = Texture::create2D(width / 16, height / 16, ResourceFormat::R11G11B10Float, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
}

void post_effects::create_tonemap_resource(const Program::DefineList& programDefines) {
    mpToneMap_ = ComputePass::create(kTonemapProg, "main", programDefines);
}

void post_effects::create_present_resource(const Program::DefineList& programDefines) {
    mpPresent_ = FullScreenPass::create(kPresentProg, programDefines);
}

void post_effects::do_bloom(RenderContext* pContext, const Sampler::SharedPtr& texSampler) {
    PROFILE("bloom");

    // extract_downsample
    {
        mpExtractAndDownsample_->getVars()["g_texSampler"] = texSampler;
        mpExtractAndDownsample_->getVars()["g_sourceTex"] = mpPingpongBuffer_[curIndx_]->getColorTexture(0);
        mpExtractAndDownsample_->getVars()["g_exposure"] = mpExposure_;
        mpExtractAndDownsample_->getVars()["g_bloomResult"] = mpBloomUAV1_[0];
        mpExtractAndDownsample_->getVars()["g_lumaResult"] = mpLumaResult_;
        mpExtractAndDownsample_->getVars()["CB"]["g_bloomThreshold"] = bloomThreshold_;
        mpExtractAndDownsample_->getVars()["CB"]["g_inverseOutputSize"] = float2(1.0f / mpLumaResult_->getWidth(), 1.0f / mpLumaResult_->getHeight());

        mpExtractAndDownsample_->execute(pContext, uint3{ mpBloomUAV1_[0]->getWidth(), mpBloomUAV1_[0]->getHeight(), 1 });
    }
    // down sample
    {
        mpDownSample_->getVars()["g_texSampler"] = texSampler;
        mpDownSample_->getVars()["g_bloomBuf"] = mpBloomUAV1_[0];
        mpDownSample_->getVars()["g_result1"] = mpBloomUAV2_[0];
        mpDownSample_->getVars()["g_result2"] = mpBloomUAV3_[0];
        mpDownSample_->getVars()["g_result3"] = mpBloomUAV4_[0];
        mpDownSample_->getVars()["g_result4"] = mpBloomUAV5_[0];
        mpDownSample_->getVars()["CB"]["g_inverseOutputSize"] = float2(1.0f / mpLumaResult_->getWidth(), 1.0f / mpLumaResult_->getHeight());
        mpDownSample_->execute(pContext, uint3{ mpBloomUAV1_[0]->getWidth() / 2, mpBloomUAV1_[0]->getHeight() / 2, 1 });
    }

    // blur
    {
        mpBlur_->getVars()["g_blurInput"] = mpBloomUAV5_[0];
        mpBlur_->getVars()["g_blurResult"] = mpBloomUAV5_[1];
        mpBlur_->execute(pContext, mpBloomUAV5_[0]->getWidth(), mpBloomUAV5_[0]->getHeight(), 1);
    }
    // up blur
    {
        do_bloom_up_blur(pContext, mpBloomUAV4_[1], mpBloomUAV4_[0], mpBloomUAV5_[1]);
        do_bloom_up_blur(pContext, mpBloomUAV3_[1], mpBloomUAV3_[0], mpBloomUAV4_[1]);
        do_bloom_up_blur(pContext, mpBloomUAV2_[1], mpBloomUAV2_[0], mpBloomUAV3_[1]);
        do_bloom_up_blur(pContext, mpBloomUAV1_[1], mpBloomUAV1_[0], mpBloomUAV2_[1]);
    }
}

void post_effects::do_bloom_up_blur(RenderContext* pContext, const Texture::SharedPtr& target, const Texture::SharedPtr& highSource, const Texture::SharedPtr& lowSource) {
    mpUpBlur_->getVars()["g_higherResBuf"] = highSource;
    mpUpBlur_->getVars()["g_lowerResBuf"] = lowSource;
    mpUpBlur_->getVars()["g_upBlurResult"] = target;
    mpUpBlur_->getVars()["g_linearBorder"] = mpUpBlurSample_;
    mpUpBlur_->getVars()["CB"]["g_inverseOutputSize"] = float2(1.0f / target->getWidth(), 1.0f / target->getHeight());
    mpUpBlur_->getVars()["CB"]["g_upsampleBlendFactor"] = upSampleBlendFactor_;
    mpUpBlur_->execute(pContext, { target->getWidth(), target->getHeight(), 1 });
}

void post_effects::do_tone_map(RenderContext* pContext, const Sampler::SharedPtr& texSampler) {
    PROFILE("tonemap");

    uint2 bufferSize = { mpPingpongBuffer_[0]->getWidth(), mpPingpongBuffer_[0]->getHeight() };

    mpToneMap_->getVars()["g_exposure"] = mpExposure_;
    mpToneMap_->getVars()["g_bloom"] = mpBloomUAV1_[1];
    mpToneMap_->getVars()["g_colorRW"] = mpPingpongBuffer_[curIndx_]->getColorTexture(0);
    mpToneMap_->getVars()["g_texSampler"] = texSampler;
    mpToneMap_->getVars()["CB"]["g_recpBuferDim"] = float2{ 1.0f / (float)bufferSize.x, 1.0f / (float)bufferSize.y };
    mpToneMap_->getVars()["CB"]["g_bloomStrength"] = bloomStrength_;

    mpToneMap_->execute(pContext, uint3{ bufferSize.x, bufferSize.y, 1 });
}

void post_effects::do_present(RenderContext* pContext, const Sampler::SharedPtr& texSampler, const Fbo::SharedPtr& pDestFbo) {
    mpPresent_->getVars()["g_colorTex"] =  mpPingpongBuffer_[curIndx_]->getColorTexture(0);
    mpPresent_->getVars()["g_texSampler"] = texSampler;
    mpPresent_->execute(pContext, pDestFbo);
}

post_effects::~post_effects() {
    // bloom
    {
        mpBloomUAV1_[0] = nullptr;
        mpBloomUAV1_[1] = nullptr;
        mpBloomUAV2_[0] = nullptr;
        mpBloomUAV2_[1] = nullptr;
        mpBloomUAV3_[0] = nullptr;
        mpBloomUAV3_[1] = nullptr;
        mpBloomUAV4_[0] = nullptr;
        mpBloomUAV4_[1] = nullptr;
        mpBloomUAV5_[0] = nullptr;
        mpBloomUAV5_[1] = nullptr;
        mpLumaResult_ = nullptr;
        mpExtractAndDownsample_ = nullptr;
        mpDownSample_ = nullptr;
        mpBlur_ = nullptr;
        mpUpBlur_ = nullptr;
        mpUpBlurSample_ = nullptr;
    }

    // tone map
    {
        mpToneMap_ = nullptr;
    }

    // present
    {
        mpPresent_ = nullptr;
    }

    mpPingpongBuffer_[0] = nullptr;
    mpPingpongBuffer_[1] = nullptr;
}

void post_effects::on_swapchain_resize(uint32_t width, uint32_t height) {
    Fbo::Desc desc = {};
    desc.setColorTarget(0, ResourceFormat::R11G11B10Float, true);
    mpPingpongBuffer_[0] = Fbo::create2D(width, height, desc);
    mpPingpongBuffer_[1] = Fbo::create2D(width, height, desc);

    // bloom
    {
        uint32_t kBloomWidth = width > 2560 ? 1280 : 640;
        uint32_t kBloomHeight = height > 1440 ? 768 : 384;

        if (!mpLumaResult_ || mpLumaResult_->getWidth() != kBloomWidth || mpLumaResult_->getHeight() != kBloomHeight) {
            rebuild_bloom_buffers(kBloomWidth, kBloomHeight);
        }
    }
}

post_effects::SharedPtr post_effects::create(const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    return SharedPtr(new post_effects(programDefines));
}

void post_effects::on_gui_render(Gui::Group& group) {
    if (group.var("Exposure", exposure_, -8.0f, 8.0f, 0.01f)) {
        g_exposure = { exposure_, 1.0f / exposure_, exposure_, 0.0f,
            initialMinLog_, initialMaxLog_, initialMaxLog_ - initialMinLog_, 1.0f / (initialMaxLog_ - initialMinLog_) };
        mpExposure_->setBlob(&g_exposure, 0, sizeof(g_exposure));
    }
    group.var("Threshold", bloomThreshold_, 0.0f, 8.0f, 0.01f);
    group.var("UpSampleBlendFactor", upSampleBlendFactor_, 0.0f, 1.0f, 0.01f);
    group.var("BloomStrength", bloomStrength_, 0.0f, 2.0f, 0.01f);
}

void post_effects::on_execute(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& texSampler) {
    PROFILE("post effects");

    do_bloom(pContext, texSampler);
    do_tone_map(pContext, texSampler);
    do_present(pContext, texSampler, pDstFbo);

    curIndx_ = (curIndx_ + 1) % 2;
}

void post_effects::do_clear(RenderContext* pRenderContext) {
    const float4 clearColor(0.f, 0.f, 0.f, 1);
    pRenderContext->clearFbo(mpPingpongBuffer_[0].get(), clearColor, 1.0f, 0, FboAttachmentType::Color);
    pRenderContext->clearFbo(mpPingpongBuffer_[1].get(), clearColor, 1.0f, 0, FboAttachmentType::Color);
}
