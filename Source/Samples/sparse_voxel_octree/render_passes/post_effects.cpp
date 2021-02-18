#include "post_effects.h"
#include "post_effects.slangh"

namespace {
    static std::string kBloomProg = "Samples/sparse_voxel_octree/render_passes/bloom.cs.slang";
    static std::string kPresentProg = "Samples/sparse_voxel_octree/render_passes/present.slang";
    static exposure_meta g_exposure = {};
}

post_effects::post_effects(const Program::DefineList& programDefines) {
    create_bloom_resource(programDefines);
}

void post_effects::create_bloom_resource(const Program::DefineList& programDefines) {
    mpExtractAndDownsample_ = ComputePass::create(kBloomProg, "extract_and_downsample", programDefines);
    mpDownSample_ = ComputePass::create(kBloomProg, "down_sample", programDefines);
    mpExposure_ = Buffer::createStructured(sizeof(exposure_meta), 1);
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

void post_effects::do_bloom(RenderContext* pContext, const Sampler::SharedPtr& texSampler) {
    PROFILE("bloom");

    g_exposure = { exposure_, 1.0f / exposure_, exposure_, 0.0f,
        initialMinLog_, initialMaxLog_, initialMaxLog_ - initialMinLog_, 1.0f / (initialMaxLog_ - initialMinLog_) };
    mpExposure_->setBlob(&g_exposure, 0, sizeof(g_exposure));

    // extract_downsample
    {
        mpExtractAndDownsample_->getVars()["g_texSampler"] = texSampler;
        mpExtractAndDownsample_->getVars()["g_sourceTex"] = mpPingpongBuffer_[curIndx_++]->getColorTexture(0);
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

    curIndx_ = (curIndx_ + 1) % 2;
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
    group.var("Exposure", exposure_, -8.0f, 8.0f, 0.25f);
    group.var("Threshold", bloomThreshold_, 0.0f, 8.0f, 0.25f);
}

void post_effects::on_execute(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& texSampler) {
    PROFILE("post effects");
    do_bloom(pContext, texSampler);
}
