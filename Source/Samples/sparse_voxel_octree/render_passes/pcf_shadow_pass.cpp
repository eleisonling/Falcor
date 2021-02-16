#include "pcf_shadow_pass.h"

namespace {
    static std::string kGenShadowMapProg = "Samples/sparse_voxel_octree/render_passes/gen_shadowmap.slang";
    static std::string kDeferredApplyProg = "Samples/sparse_voxel_octree/render_passes/deferred_apply.slang";
    static std::string kMainLight = "Main Light";
}

pcf_shadow_pass::pcf_shadow_pass(const Scene::SharedPtr& pScene, const Program::Desc& genMapProgDesc, const Program::Desc& applyProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(genMapProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);

    auto pApplyProg = GraphicsProgram::create(applyProgDesc, programDefines);
    mpApplyPass_ = FullScreenPass::create(applyProgDesc, programDefines);
    rebuild_shadowmap_buffers();
}

void pcf_shadow_pass::rebuild_shadowmap_buffers() {
    Fbo::Desc desc = {};
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);
    mpShadowMap_ = Fbo::create2D(mpSize_.x, mpSize_.y, desc);
    smChanged_ = true;
}

pcf_shadow_pass::~pcf_shadow_pass() {
    mpScene_ = nullptr;
    mpShadowMap_ = nullptr;
    mpSceneFbo_ = nullptr;
    mpApplyPass_ = nullptr;
}

pcf_shadow_pass::SharedPtr pcf_shadow_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::Desc d_genMap;
    d_genMap.addShaderLibrary(kGenShadowMapProg).vsEntry("vs_main").psEntry("ps_main");

    Program::Desc d_apply;
    d_apply.addShaderLibrary(kDeferredApplyProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new pcf_shadow_pass(pScene, d_genMap, d_apply, dl));
}

void pcf_shadow_pass::on_gui_render(Gui::Group& group) {
    if (group.var("ShadowMap Resolution", mpSize_)) rebuild_shadowmap_buffers();
    group.var("PCF kernel size", pcf_kernel_size_);
}

void pcf_shadow_pass::generate_shadowmap(RenderContext* pContext) {
    if (!mpScene_) return;
    
    mpVars["CB"]["g_light_view_proj"] = lightViewProj_;
    pContext->clearFbo(mpShadowMap_.get(), { 0, 0, 0, 0}, 1, 0, FboAttachmentType::All);
    mpState->setFbo(mpShadowMap_);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get());
}

void pcf_shadow_pass::deferred_apply(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    auto& var = mpApplyPass_->getVars();
    var["g_shadowMap"] = mpShadowMap_->getDepthStencilTexture();
    var["g_sceneColor"] = mpSceneFbo_->getColorTexture(0);
    var["g_sceneDepth"] = mpSceneFbo_->getDepthStencilTexture();
    var["CB"]["g_pcf_kernel_size"] = pcf_kernel_size_;
    var["CB"]["g_light_viewProj"] = lightViewProj_;
    mpApplyPass_->execute(pContext, pDstFbo);
}

bool pcf_shadow_pass::refresh_rebuild() {

    bool needRefresh = false;

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

    return needRefresh;
}

