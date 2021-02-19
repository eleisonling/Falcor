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
#include "sparse_voxel_octree.h"
uint32_t mSampleGuiWidth = 250;
uint32_t mSampleGuiHeight = 200;
uint32_t mSampleGuiPositionX = 20;
uint32_t mSampleGuiPositionY = 40;

namespace {
    static const std::string kDefaultScene = "sponza/sponza.pyscene";
    static const std::string kRasterProg = "Samples/sparse_voxel_octree/render_passes/lighting.ps.slang";

    enum class final_output_type {
        defulat_type,
        debug_projection,
        debug_volumetric,

        max_count,
    };

    const Gui::DropdownList kFinalOutputType = {
        { (uint32_t)final_output_type::defulat_type, "defulat_type" },
        { (uint32_t)final_output_type::debug_projection, "debug_projection" },
        { (uint32_t)final_output_type::debug_volumetric, "debug_volumetric" },
    };

}

void sparse_voxel_octree::onGuiRender(Gui* pGui) {
    Gui::Window w(pGui, "sparse voxel octree", { 250, 200 });
    std::string msg = gpFramework->getFrameRate().getMsg(gpFramework->isVsyncEnabled());
    w.text(msg);

    gpFramework->renderGlobalUI(pGui);
    if (mpScene_) mpScene_->renderUI(w);

    auto shadowGroup = Gui::Group(pGui, "ShadowPass");
    mpShadow_->on_gui_render(shadowGroup);

    auto projectionDebugGroup = Gui::Group(pGui, "Projection Debug");
    if (projectionDebugGroup.open()) {
        mpDeubgProjection_->on_gui_render(projectionDebugGroup);
    }

    auto volumetricGroup = Gui::Group(pGui, "Volumetric");
    if (volumetricGroup.open()) {
        mpVolumetric_->on_gui_render(volumetricGroup);
    }

    auto postEffects = Gui::Group(pGui, "Post Effects");
    if (postEffects.open()) {
        mpPostEffects_->on_gui_render(postEffects);
    }

    // final output
    auto finalOutputGroup = Gui::Group(pGui, "Final Output");
    if (finalOutputGroup.open()) {
        finalOutputGroup.dropdown("Output Type", kFinalOutputType, finalOutputType_);
    }
}

void sparse_voxel_octree::load_scene(const std::string& filename, const Fbo* pTargetFbo) {
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

void sparse_voxel_octree::onLoad(RenderContext* pRenderContext) {
    load_scene(kDefaultScene, gpFramework->getTargetFbo().get());
    mpRasterPass_ = RasterScenePass::create(mpScene_, kRasterProg, "", "main");
    mpDeubgProjection_ = projection_debug_pass::create(mpScene_);
    mpVolumetric_ = volumetric_pass::create(mpScene_);
    mpShadow_ = pcf_shadow_pass::create(mpScene_);
    mpPostEffects_ = post_effects::create();

    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpTextureSampler_ = Sampler::create(desc);

}

void sparse_voxel_octree::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) {
    const float4 clearColor(0.f, 0.f, 0.f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpGBufferFbo_.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    mpPostEffects_->do_clear(pRenderContext);

    if (mpScene_) {
        mpScene_->update(pRenderContext, gpFramework->getGlobalClock().getTime());

        if (mpVolumetric_->need_refresh()) {
            mpVolumetric_->volumetric_scene(pRenderContext, mpGBufferFbo_);
        }

        switch ((final_output_type)finalOutputType_)
        {
        case final_output_type::debug_projection:
            mpDeubgProjection_->debug_scene(pRenderContext, pTargetFbo);
            break;
        case final_output_type::debug_volumetric:
            mpVolumetric_->debug_scene(pRenderContext, pTargetFbo);
            break;
        case final_output_type::defulat_type:
        default:
            if (mpShadow_->refresh_rebuild()) {
                mpShadow_->generate_shadowmap(pRenderContext);
            }

            mpRasterPass_->renderScene(pRenderContext, mpGBufferFbo_);
            mpShadow_->deferred_apply(pRenderContext, mpGBufferFbo_, mpPostEffects_->get_fbo(), mpTextureSampler_);
            mpPostEffects_->on_execute(pRenderContext, pTargetFbo, mpTextureSampler_);
            break;
        }
    }
}

void sparse_voxel_octree::onShutdown() {
    mpRasterPass_ = nullptr;
    mpDeubgProjection_ = nullptr;
    mpVolumetric_ = nullptr;
    mpScene_ = nullptr;
    mpGBufferFbo_ = nullptr;
    mpPostEffects_ = nullptr;
    mpTextureSampler_ = nullptr;
}

bool sparse_voxel_octree::onKeyEvent(const KeyboardEvent& keyEvent) {
    if (mpScene_) {
        mpRasterPass_->onKeyEvent(keyEvent);
    }
    return false;
}

bool sparse_voxel_octree::onMouseEvent(const MouseEvent& mouseEvent) {
    if (mpScene_) {
         mpRasterPass_->onMouseEvent(mouseEvent);
    }
    return false;
}

void sparse_voxel_octree::onHotReload(HotReloadFlags reloaded) {
}

void sparse_voxel_octree::onResizeSwapChain(uint32_t width, uint32_t height) {
    const auto& bkFbo = gpDevice->getSwapChainFbo();
    Fbo::Desc desc = bkFbo->getDesc();
    desc.setColorTarget(0, ResourceFormat::R11G11B10Float, true);
    mpGBufferFbo_ = Fbo::create2D(width, height, desc);
    mpPostEffects_->on_swapchain_resize(width, height);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    sparse_voxel_octree::UniquePtr pRenderer = std::make_unique<sparse_voxel_octree>();
    SampleConfig config;
    config.windowDesc.title = "sparse voxel octree";
    config.windowDesc.resizableWindow = true;
    Sample::run(config, pRenderer);
    return 0;
}
