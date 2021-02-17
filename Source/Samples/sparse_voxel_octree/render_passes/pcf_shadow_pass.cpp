#include "pcf_shadow_pass.h"

namespace {
    static std::string kGenShadowMapProg = "Samples/sparse_voxel_octree/render_passes/gen_shadowmap.slang";
    static std::string kDeferredApplyProg = "Samples/sparse_voxel_octree/render_passes/deferred_apply.slang";
    static std::string kMainLight = "Main Light";
}

pcf_shadow_pass::pcf_shadow_pass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc,Program::DefineList& programDefines)
    : BaseGraphicsPass(genMapProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);

    mpApplyPass_ = FullScreenPass::create(kDeferredApplyProg, programDefines);

    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpTextureSampler_ = Sampler::create(desc);

    desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border).setBorderColor(float4(1.0f));
    desc.setLodParams(0.f, 0.f, 0.f);
    desc.setComparisonMode(Sampler::ComparisonMode::LessEqual);
    mpPCFSampler_ = Sampler::create(desc);

    rebuild_shadowmap_buffers();
}

void pcf_shadow_pass::rebuild_shadowmap_buffers() {
    Fbo::Desc desc = {};
    desc.setDepthStencilTarget(ResourceFormat::D32Float);
    mpShadowMap_ = Fbo::create2D(mpSize_.x, mpSize_.y, desc);
    smChanged_ = true;
}

void pcf_shadow_pass::rebuild_shadow_matrix(float3 lightDir, const AABB& bounds) {
    float3 up = glm::normalize(glm::cross(glm::cross(cachedMainLightDir_, { 0,1,0 }), cachedMainLightDir_));
    float4x4 lightView = glm::lookAt(bounds.center() - bounds.radius() * cachedMainLightDir_, bounds.center(), up);
    float3 viewMax = lightView * float4(bounds.maxPoint, 1.0f);
    float3 viewMin = lightView * float4(bounds.minPoint, 1.0f);
    float hWidth = glm::abs(viewMax - viewMin).x / 2.0f;
    float hHeight = glm::abs(viewMax - viewMin).y / 2.0f;

    float4x4 lightProj = {};
    if (hWidth > hHeight) {
        float scale = (float)mpSize_.y / (float)mpSize_.x;
        lightProj = glm::ortho(-hWidth, hWidth, -hWidth * scale, hWidth * scale, 0.1f, 4.0f * bounds.radius());
    } else {
        float scale = (float)mpSize_.x / (float)mpSize_.y;
        lightProj = glm::ortho(-hHeight * scale, hHeight * scale, -hHeight, hHeight, 0.1f, 4.0f * bounds.radius());
    }

    lightViewProj_ = lightProj * lightView;
    float4x4 affine = glm::scale(glm::identity<float4x4>(), { 0.5f, 0.5f, 1.f });
    affine = glm::translate(affine, { 0.5f, 0.5f, 0.0f });
    shadowMatrix_ = affine * lightViewProj_;
}

pcf_shadow_pass::~pcf_shadow_pass() {
    mpScene_ = nullptr;
    mpShadowMap_ = nullptr;
    mpApplyPass_ = nullptr;
    mpPCFSampler_ = nullptr;
    mpTextureSampler_ = nullptr;
}

pcf_shadow_pass::SharedPtr pcf_shadow_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::Desc d_genMap;
    d_genMap.addShaderLibrary(kGenShadowMapProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new pcf_shadow_pass(pScene, d_genMap, dl));
}

void pcf_shadow_pass::on_gui_render(Gui::Group& group) {
    if (group.var("ShadowMap Resolution", mpSize_)) rebuild_shadowmap_buffers();
    group.var("PCF kernel size", pcf_kernel_size_);
}

void pcf_shadow_pass::generate_shadowmap(RenderContext* pContext) {
    if (!mpScene_) return;

    PROFILE("generate shadow map");
    
    mpVars["CB"]["g_light_view_proj"] = lightViewProj_;
    mpVars->setParameterBlock("gScene", mpScene_->getParameterBlock());
    pContext->clearFbo(mpShadowMap_.get(), { 0, 0, 0, 0}, 1, 0, FboAttachmentType::All);
    mpState->setFbo(mpShadowMap_);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get());
}

void pcf_shadow_pass::deferred_apply(RenderContext* pContext,const Fbo::SharedPtr& pSceneFbo, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("apply shadow map");

    auto& var = mpApplyPass_->getVars();
    var->setParameterBlock("gScene", mpScene_->getParameterBlock());
    var["g_shadowMap"] = mpShadowMap_->getDepthStencilTexture();
    var["g_sceneColor"] = pSceneFbo->getColorTexture(0);
    var["g_sceneDepth"] = pSceneFbo->getDepthStencilTexture();
    var["g_texSampler"] = mpTextureSampler_;
    var["g_pcfSampler"] = mpPCFSampler_;
    var["CB"]["g_pcf_kernel_size"] = pcf_kernel_size_;
    var["CB"]["g_shadow_matrix"] = shadowMatrix_;
    var["CB"]["g_screen_dimension"] = float2{ pSceneFbo->getWidth(), pSceneFbo->getHeight() };
    mpApplyPass_->execute(pContext, pDstFbo);
}

bool pcf_shadow_pass::refresh_rebuild() {

    bool needRefresh = false;
    auto& bounds = mpScene_->getSceneBounds();
    float4x4 lightView = {};
    float4x4 lightProj = {};

    if (mpScene_) {
        auto mainLight = std::static_pointer_cast<DirectionalLight, Light>(mpScene_->getLightByName(kMainLight));
        if (mainLight) {
            const float3& dir = mainLight->getWorldDirection();
            if (cachedMainLightDir_ != dir) {
                needRefresh |= true;
                cachedMainLightDir_ = dir;
            }
        }
    }

    needRefresh |= smChanged_;
    smChanged_ = false;

    if (needRefresh) {
        rebuild_shadow_matrix(cachedMainLightDir_, bounds);
    }

    return needRefresh;
}

