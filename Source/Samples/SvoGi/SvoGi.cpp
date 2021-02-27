/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "SvoGi.h"
uint32_t mSampleGuiWidth = 250;
uint32_t mSampleGuiHeight = 200;
uint32_t mSampleGuiPositionX = 20;
uint32_t mSampleGuiPositionY = 40;

namespace {
    static const std::string kDefaultScene = "sponza/sponza.pyscene";
    static const std::string kRasterProg = "Samples/SvoGi/Shaders/FinalShading.ps.slang";

    enum class FinalType {
        Defulat,
        debug_projection,
        Volumetric,

        max_count,
    };

    const Gui::DropdownList kFinalOutputType = {
        { (uint32_t)FinalType::Defulat, "Defulat" },
        { (uint32_t)FinalType::Volumetric, "Volumetric" },
    };

}

void SvoGi::onGuiRender(Gui* pGui) {
    Gui::Window w(pGui, "SvoGi", { 250, 200 });
    std::string msg = gpFramework->getFrameRate().getMsg(gpFramework->isVsyncEnabled());
    w.text(msg);

    gpFramework->renderGlobalUI(pGui);
    if (mpScene_) mpScene_->renderUI(w);

    auto shadowGroup = Gui::Group(pGui, "ShadowPass");
    mpShadowMap_->on_gui(shadowGroup);

    auto volumetricGroup = Gui::Group(pGui, "Voxelization");
    if (volumetricGroup.open()) {
        mpVolumetric_->on_gui(volumetricGroup);
    }

    auto voxelVisualGroup = Gui::Group(pGui, "Voxelization Visual");
    if (voxelVisualGroup.open()) {
        mpVoxelVisualizer_->on_gui(voxelVisualGroup);
    }

    auto postEffects = Gui::Group(pGui, "Post Effect");
    if (postEffects.open()) {
        mpPostEffects_->on_gui(postEffects);
    }

    // final output
    auto finalOutputGroup = Gui::Group(pGui, "Final Output");
    if (finalOutputGroup.open()) {
        finalOutputGroup.dropdown("Output Type", kFinalOutputType, mFinalOutputType_);
    }
}

void SvoGi::load_scene(const std::string& filename, const Fbo* pTargetFbo) {
    mpScene_ = Scene::create(filename);
    if (!mpScene_) return;

    mpMainCam_ = mpScene_->getCamera();

    // Update the controllers
    float radius = mpScene_->getSceneBounds().radius();
    mpScene_->setCameraSpeed(radius * 0.25f);
    float nearZ = std::max(0.1f, radius / 750.0f);
    float farZ = radius * 10;
    mpMainCam_->setDepthRange(nearZ, farZ);
    mpMainCam_->setAspectRatio((float)pTargetFbo->getWidth() / (float)pTargetFbo->getHeight());
}

void SvoGi::normal_render(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) {
    mpFinalShading_->getVars()["texShadowMap"] = mpShadowMap_->get_shadow_map();
    mpFinalShading_->getVars()["spPcfSampler"] = mpShadowMap_->get_shadow_sampler();
    mpFinalShading_->getVars()["PerFrameCB"]["matShadowMatrix"] = mpShadowMap_->get_shadow_matrix();
    mpFinalShading_->getVars()["PerFrameCB"]["iPcfKernel"] = mpShadowMap_->get_pcf_kernel();
    mpFinalShading_->getVars()["PerFrameCB"]["iShadowMapDimension"] = mpShadowMap_->get_shadow_map_dimension();
    mpFinalShading_->renderScene(pRenderContext, mpHDRFbo_);

    mpPostEffects_->set_input(mpHDRFbo_->getColorTexture(0)->getSRV());
    mpPostEffects_->on_render(pRenderContext, pTargetFbo, mpTextureSampler_);
}

void SvoGi::onLoad(RenderContext* pRenderContext) {
    load_scene(kDefaultScene, gpFramework->getTargetFbo().get());
    mpFinalShading_ = RasterScenePass::create(mpScene_, kRasterProg, "", "main");
    mpVolumetric_ = VoxlizationPass::create(mpScene_);
    mpVoxelVisualizer_ = VoxelVisualizer::create(mpScene_);
    mpShadowMap_ = ShadowPass::create(mpScene_);
    mpPostEffects_ = PostEffect::create();

    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpTextureSampler_ = Sampler::create(desc);

    desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Linear).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpVoxelSampler_ = Sampler::create(desc);

}

void SvoGi::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) {
    const float4 clearColor(0.f, 0.f, 0.f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpHDRFbo_.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    mpScene_->update(pRenderContext, gpFramework->getGlobalClock().getTime());

    if (mpVolumetric_->need_refresh()) {
        mpVolumetric_->on_render(pRenderContext, mpHDRFbo_);
    }

    mpShadowMap_->on_render(pRenderContext);

    switch ((FinalType)mFinalOutputType_) {
    case FinalType::Volumetric:
        mpVoxelVisualizer_->set_voxelization_meta(mpVolumetric_->get_voxelization_meta());
        mpVoxelVisualizer_->set_voxel_texture(mpVolumetric_->get_albedo_voxel_texture());
        mpVoxelVisualizer_->set_svo_node_buffer(mpVolumetric_->get_svo_node_buffer());
        mpVoxelVisualizer_->on_render(pRenderContext, pTargetFbo, mpVoxelSampler_);
        break;
    case FinalType::Defulat:
    default:
        normal_render(pRenderContext, pTargetFbo);
        break;
    }
}

void SvoGi::onShutdown() {
    mpFinalShading_ = nullptr;
    mpVolumetric_ = nullptr;
    mpVoxelVisualizer_ = nullptr;
    mpScene_ = nullptr;
    mpHDRFbo_ = nullptr;
    mpPostEffects_ = nullptr;
    mpTextureSampler_ = nullptr;
    mpVoxelSampler_ = nullptr;
}

bool SvoGi::onKeyEvent(const KeyboardEvent& keyEvent) {
    if (mpScene_) {
        mpFinalShading_->onKeyEvent(keyEvent);
    }
    return false;
}

bool SvoGi::onMouseEvent(const MouseEvent& mouseEvent) {
    if (mpScene_) {
         mpFinalShading_->onMouseEvent(mouseEvent);
    }
    return false;
}

void SvoGi::onHotReload(HotReloadFlags reloaded) {
}

void SvoGi::onResizeSwapChain(uint32_t width, uint32_t height) {
    const auto& bkFbo = gpDevice->getSwapChainFbo();
    Fbo::Desc desc = bkFbo->getDesc();
    desc.setColorTarget(0, ResourceFormat::R11G11B10Float, true);
    mpHDRFbo_ = Fbo::create2D(width, height, desc);
    mpPostEffects_->on_resize(width, height);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    SvoGi::UniquePtr pRenderer = std::make_unique<SvoGi>();
    SampleConfig config;
    config.windowDesc.title = "SvoGi";
    config.windowDesc.resizableWindow = true;
    Sample::run(config, pRenderer);
    return 0;
}
