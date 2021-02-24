#include "light_injection.h"
#include "injection_meta.slangh"

namespace {
    static std::string kInjectLightProg = "Samples/sparse_voxel_octree/render_passes/light_injection.slang";
}

light_injection::light_injection(const Scene::SharedPtr& pScene, Program::DefineList& programDefines)
    : mpScene_(pScene){
    create_light_injection_shaders(programDefines);
    create_light_injection_resources();
}

void light_injection::create_light_injection_shaders(Program::DefineList& programDefines) {
    mpInjection_ = ComputePass::create(kInjectLightProg, "light_injection", programDefines);
}

void light_injection::create_light_injection_resources() {
    mpRadius_ = Texture::create3D(RESOLUTION_WITH_BORDER * FACE_COUNT, RESOLUTION_WITH_BORDER * MIP_COUNT, RESOLUTION_WITH_BORDER, ResourceFormat::RGBA16Float, 1, nullptr,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
}

light_injection::~light_injection() {
    mpScene_ = nullptr;
    mpInjection_ = nullptr;
    mpDownSample_ = nullptr;
    mpRadius_ = nullptr;
}

light_injection::SharedPtr light_injection::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return light_injection::SharedPtr(new light_injection(pScene, dl));
}

void light_injection::on_gui_render(Gui::Group& group) {

}

void light_injection::on_inject_light(RenderContext* pContext, const Texture::SharedPtr& pShadowmap, const float4x4& shadowMatrix, const Texture::SharedPtr& pAlbedoTexture, const Texture::SharedPtr& pNormalTexture, const svo_meta& meta) {
    pContext->clearTexture(mpRadius_.get());

    mpInjection_->getVars()["CB"]["g_svoMeta"].setBlob(meta);
    mpInjection_->getVars()["CB"]["g_shadow_matrix"] = shadowMatrix;
    mpInjection_->getVars()["g_albedo"] = pAlbedoTexture;
    mpInjection_->getVars()["g_normal"] = pNormalTexture;
    mpInjection_->getVars()["g_normal"] = pShadowmap;
    mpInjection_->getVars()["g_radius"] = mpRadius_;
    mpInjection_->execute(pContext, meta.CellDim);
}

void light_injection::on_down_sampler(RenderContext* pContext) {

}
