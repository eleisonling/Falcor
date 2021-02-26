#include "post_effect.h"
#include "../shaders/post_effects.slangh"

namespace {
    static std::string kBloomProg = "samples/sparse_voxel_octree/shaders/bloom.cs.slang";
    static std::string kTonemapProg = "samples/sparse_voxel_octree/shaders/tonemap.cs.slang";
    static std::string kPresentProg = "Samples/sparse_voxel_octree/shaders/present.ps.slang";
    static exposure_meta bufExposure = {};
}

post_effect::post_effect(const Program::DefineList& programDefines) {
    mpExposure_ = Buffer::createStructured(sizeof(exposure_meta), 1);
    float exp = glm::pow(2.0f, mExpExposure_);
    bufExposure = { exp, 1.0f / exp, mInitialMinLog_, mInitialMaxLog_, mInitialMaxLog_ - mInitialMinLog_, 1.0f / (mInitialMaxLog_ - mInitialMinLog_) };
    mpExposure_->setBlob(&bufExposure, 0, sizeof(bufExposure));

    create_bloom_resource(programDefines);
    create_tonemap_resource(programDefines);
    create_present_resource(programDefines);
}

void post_effect::create_bloom_resource(const Program::DefineList& programDefines) {
    mpExtractAndDownsample_ = ComputePass::create(kBloomProg, "extract_and_downsample", programDefines);
    mpDownSample_ = ComputePass::create(kBloomProg, "down_sample", programDefines);
    mpBlur_ = ComputePass::create(kBloomProg, "blur", programDefines);
    mpUpBlur_ = ComputePass::create(kBloomProg, "up_blur", programDefines);
    
    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point)
        .setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border)
        .setBorderColor({ 0.0f,0.0f,0.0f,0.0f });
    mpUpBlurSampler_ = Sampler::create(desc);
}

void post_effect::rebuild_bloom_buffers(uint32_t width, uint32_t height) {
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

void post_effect::create_tonemap_resource(const Program::DefineList& programDefines) {
    mpToneMap_ = ComputePass::create(kTonemapProg, "main", programDefines);
}

void post_effect::create_present_resource(const Program::DefineList& programDefines) {
    mpPresent_ = FullScreenPass::create(kPresentProg, programDefines);
}

void post_effect::do_bloom(RenderContext* pContext) {
    PROFILE("bloom");

    // extract_downsample
    {
        mpExtractAndDownsample_->getVars()["spTexSampler"] = mpTexSampler_;
        mpExtractAndDownsample_->getVars()["texSource"] = mpPingpongBuffer_[mCurIndx_]->getColorTexture(0);
        mpExtractAndDownsample_->getVars()["bufExposure"] = mpExposure_;
        mpExtractAndDownsample_->getVars()["texBloomResult"] = mpBloomUAV1_[0];
        mpExtractAndDownsample_->getVars()["texLumaResult"] = mpLumaResult_;
        mpExtractAndDownsample_->getVars()["CB"]["fBloomThreshold"] = mBloomThreshold_;
        mpExtractAndDownsample_->getVars()["CB"]["fInverseOutputSize"] = float2(1.0f / mpLumaResult_->getWidth(), 1.0f / mpLumaResult_->getHeight());

        mpExtractAndDownsample_->execute(pContext, uint3{ mpBloomUAV1_[0]->getWidth(), mpBloomUAV1_[0]->getHeight(), 1 });
    }
    // down sample
    {
        mpDownSample_->getVars()["spTexSampler"] = mpTexSampler_;
        mpDownSample_->getVars()["texBloomBuf"] = mpBloomUAV1_[0];
        mpDownSample_->getVars()["texResult1"] = mpBloomUAV2_[0];
        mpDownSample_->getVars()["texResult2"] = mpBloomUAV3_[0];
        mpDownSample_->getVars()["texResult3"] = mpBloomUAV4_[0];
        mpDownSample_->getVars()["texResult4"] = mpBloomUAV5_[0];
        mpDownSample_->getVars()["CB"]["fInverseOutputSize"] = float2(1.0f / mpLumaResult_->getWidth(), 1.0f / mpLumaResult_->getHeight());
        mpDownSample_->execute(pContext, uint3{ mpBloomUAV1_[0]->getWidth() / 2, mpBloomUAV1_[0]->getHeight() / 2, 1 });
    }

    // blur
    {
        mpBlur_->getVars()["texBlurInput"] = mpBloomUAV5_[0];
        mpBlur_->getVars()["texBlurResult"] = mpBloomUAV5_[1];
        mpBlur_->getVars()["spLinearBorder"] = mpUpBlurSampler_;
        mpBlur_->getVars()["CB"]["fInverseOutputSize"] = float2(1.0f / mpBloomUAV5_[0]->getWidth(), 1.0f / mpBloomUAV5_[0]->getHeight());
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

void post_effect::do_bloom_up_blur(RenderContext* pContext, const Texture::SharedPtr& pTarget, const Texture::SharedPtr& pHighSource, const Texture::SharedPtr& pLowSource) {
    mpUpBlur_->getVars()["texHigherResBuf"] = pHighSource;
    mpUpBlur_->getVars()["texLowerResBuf"] = pLowSource;
    mpUpBlur_->getVars()["texUpBlurResult"] = pTarget;
    mpUpBlur_->getVars()["spLinearBorder"] = mpUpBlurSampler_;
    mpUpBlur_->getVars()["CB"]["fInverseOutputSize"] = float2(1.0f / pTarget->getWidth(), 1.0f / pTarget->getHeight());
    mpUpBlur_->getVars()["CB"]["fUpsampleBlendFactor"] = mUpSampleBlendFactor_;
    mpUpBlur_->execute(pContext, { pTarget->getWidth(), pTarget->getHeight(), 1 });
}

void post_effect::do_tone_map(RenderContext* pContext) {
    PROFILE("tonemap");

    uint2 bufferSize = { mpPingpongBuffer_[0]->getWidth(), mpPingpongBuffer_[0]->getHeight() };

    mpToneMap_->getVars()["bufExposure"] = mpExposure_;
    mpToneMap_->getVars()["texBloom"] = mpBloomUAV1_[1];
    mpToneMap_->getVars()["texColorRW"] = mpPingpongBuffer_[mCurIndx_]->getColorTexture(0);
    mpToneMap_->getVars()["spTexSampler"] = mpTexSampler_;
    mpToneMap_->getVars()["CB"]["g_recpBuferDim"] = float2{ 1.0f / (float)bufferSize.x, 1.0f / (float)bufferSize.y };
    mpToneMap_->getVars()["CB"]["g_bloomStrength"] = mBloomStrength_;

    mpToneMap_->execute(pContext, uint3{ bufferSize.x, bufferSize.y, 1 });
}

void post_effect::do_present(RenderContext* pContext, const Fbo::SharedPtr& pDestFbo) {
    PROFILE("present");

    mpPresent_->getVars()["texColor"] =  mpPingpongBuffer_[mCurIndx_]->getColorTexture(0);
    mpPresent_->getVars()["spTexSampler"] = mpTexSampler_;
    mpPresent_->execute(pContext, pDestFbo);
}

post_effect::~post_effect() {
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
        mpUpBlurSampler_ = nullptr;
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
    mpInput_ = nullptr;
    mpTexSampler_ = nullptr;
}

void post_effect::on_resize(uint32_t width, uint32_t height) {
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

post_effect::SharedPtr post_effect::create(const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    return SharedPtr(new post_effect(programDefines));
}

void post_effect::on_gui(Gui::Group& group) {
    if (group.var("Exposure(exp)", mExpExposure_, mExpMinExposure_, mExpMaxExposure_, 0.25f)) {
        float exp = glm::pow(2.0f, mExpExposure_);
        bufExposure = { exp, 1.0f / exp, mInitialMinLog_, mInitialMaxLog_, mInitialMaxLog_ - mInitialMinLog_, 1.0f / (mInitialMaxLog_ - mInitialMinLog_) };
        mpExposure_->setBlob(&bufExposure, 0, sizeof(bufExposure));
    }
    group.var("Min Exposure(exp)", mExpMinExposure_, -8.0f, 0.0f, 0.25f);
    group.var("Max Exposure(exp)", mExpMaxExposure_, 0.0f, 8.0f, 0.25f);
    group.var("Threshold", mBloomThreshold_, 0.0f, 8.0f, 0.01f);
    group.var("UpSampleBlendFactor", mUpSampleBlendFactor_, 0.0f, 1.0f, 0.1f);
    group.var("BloomStrength", mBloomStrength_, 0.0f, 2.0f, 0.05f);
}

void post_effect::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& texSampler) {
    PROFILE("post effects");

    do_clear(pContext);
    pContext->blit(mpInput_, mpPingpongBuffer_[mCurIndx_]->getRenderTargetView(0));
    do_bloom(pContext);
    do_tone_map(pContext);
    do_present(pContext, pDstFbo);
    mCurIndx_ = (mCurIndx_ + 1) % 2;
}

void post_effect::do_clear(RenderContext* pRenderContext) {
    const float4 clearColor(0.f, 0.f, 0.f, 1);
    pRenderContext->clearFbo(mpPingpongBuffer_[0].get(), clearColor, 1.0f, 0, FboAttachmentType::Color);
    pRenderContext->clearFbo(mpPingpongBuffer_[1].get(), clearColor, 1.0f, 0, FboAttachmentType::Color);
}
