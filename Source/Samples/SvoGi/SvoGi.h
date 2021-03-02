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
#pragma once
#include "Falcor.h"
#include "RenderPass/VoxlizationPass.h"
#include "RenderPass/VoxelVisualizer.h"
#include "RenderPass/ShadowPass.h"
#include "RenderPass/PostEffect.h"

using namespace Falcor;

class SvoGi : public IRenderer {
public:
    void onLoad(RenderContext* pRenderContext) override;
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onShutdown() override;
    void onResizeSwapChain(uint32_t width, uint32_t height) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onHotReload(HotReloadFlags reloaded) override;
    void onGuiRender(Gui* pGui) override;

private:

    void load_scene(const std::string& filename, const Fbo* pTargetFbo);
    void normal_render(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo);

    Scene::SharedPtr mpScene_ = nullptr;
    Camera::SharedPtr mpMainCam_ = nullptr;

    ShadowPass::SharedPtr mpShadowMap_ = nullptr;
    RasterScenePass::SharedPtr mpFinalShading_ = nullptr;
    PostEffect::SharedPtr mpPostEffects_ = nullptr;
    Fbo::SharedPtr mpHDRFbo_ = nullptr;

    VoxlizationPass::SharedPtr mpVolumetric_ = nullptr;
    VoxelVisualizer::SharedPtr mpVoxelVisualizer_ = nullptr;

    uint32_t mFinalOutputType_ = 1;
    Sampler::SharedPtr mpTextureSampler_ = nullptr;
    Sampler::SharedPtr mpVoxelSampler_ = nullptr;
};