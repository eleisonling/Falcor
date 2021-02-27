#include "ShadowPass.h"

namespace {
    static std::string kGenShadowMapProg = "Samples/SvoGi/Shaders/Shadowmap.slang";
    static std::string kMainLight = "Main Light";
}

ShadowPass::ShadowPass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc,Program::DefineList& programDefines)
    : BaseGraphicsPass(genMapProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);

    Sampler::Desc desc = {};
    desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border).setBorderColor(float4(1.0f));
    desc.setLodParams(0.f, 0.f, 0.f);
    desc.setComparisonMode(Sampler::ComparisonMode::LessEqual);
    mpPCFSampler_ = Sampler::create(desc);


    RasterizerState::Desc rasterDesc{};
    rasterDesc.setDepthBias(100000, 1.0);
    auto rasterState = RasterizerState::create(rasterDesc);
    mpState->setRasterizerState(rasterState);

    rebuild_shadowmap_buffers();
}

void ShadowPass::rebuild_shadowmap_buffers() {
    Fbo::Desc desc = {};
    desc.setDepthStencilTarget(ResourceFormat::D32Float);
    mpShadowMap_ = Fbo::create2D(mDimension_.x, mDimension_.y, desc);
}

void ShadowPass::rebuild_shadow_matrix(float3 lightDir, const AABB& bounds) {
    float3 up = glm::normalize(glm::cross(glm::cross(lightDir, { 0,1,0 }), lightDir));
    float4x4 lightView = glm::lookAt(bounds.center() - bounds.radius() * lightDir, bounds.center(), up);
    float3 viewMax = lightView * float4(bounds.maxPoint, 1.0f);
    float3 viewMin = lightView * float4(bounds.minPoint, 1.0f);
    float hWidth = glm::abs(viewMax - viewMin).x / 2.0f;
    float hHeight = glm::abs(viewMax - viewMin).y / 2.0f;

    float4x4 lightProj = {};
    if (hWidth > hHeight) {
        float scale = (float)mDimension_.y / (float)mDimension_.x;
        lightProj = glm::ortho(-hWidth, hWidth, -hWidth * scale, hWidth * scale, 0.1f, 4.0f * bounds.radius());
    } else {
        float scale = (float)mDimension_.x / (float)mDimension_.y;
        lightProj = glm::ortho(-hHeight * scale, hHeight * scale, -hHeight, hHeight, 0.1f, 4.0f * bounds.radius());
    }

    mShadowMatrix_ = lightProj * lightView;
}

ShadowPass::~ShadowPass() {
    mpScene_ = nullptr;
    mpShadowMap_ = nullptr;
    mpPCFSampler_ = nullptr;
}

ShadowPass::SharedPtr ShadowPass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::Desc d_genMap;
    d_genMap.addShaderLibrary(kGenShadowMapProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new ShadowPass(pScene, d_genMap, dl));
}

void ShadowPass::on_gui(Gui::Group& group) {
    if (group.var("ShadowMap Resolution", mDimension_)) rebuild_shadowmap_buffers();
    group.var("PCF kernel size", mPcfKernel_);
}

void ShadowPass::on_render(RenderContext* pContext) {
    PROFILE("generate shadow map");
    
    auto& bounds = mpScene_->getSceneBounds();
    auto mainLight = std::static_pointer_cast<DirectionalLight, Light>(mpScene_->getLightByName(kMainLight));
    rebuild_shadow_matrix(mainLight->getWorldDirection(), bounds);

    mpVars["CB"]["g_light_view_proj"] = mShadowMatrix_;
    mpVars->setParameterBlock("gScene", mpScene_->getParameterBlock());
    pContext->clearFbo(mpShadowMap_.get(), { 0, 0, 0, 0}, 1, 0, FboAttachmentType::Depth);
    mpState->setFbo(mpShadowMap_);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
}

