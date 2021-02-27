#include "LightInjection.h"
#include "../Shaders/LightInjectionMeta.slangh"

namespace {
    static std::string kInjectLightProg = "Samples/SvoGi/Shaders/LightInjection.slang";
}

LightInjection::LightInjection(const Scene::SharedPtr& pScene, Program::DefineList& programDefines)
    : mpScene_(pScene){
    create_light_injection_shaders(programDefines);
    create_light_injection_resources();
}

void LightInjection::create_light_injection_shaders(Program::DefineList& programDefines) {
    mpInjection_ = ComputePass::create(kInjectLightProg, "light_injection", programDefines);
}

void LightInjection::create_light_injection_resources() {
    mpRadius_ = Texture::create3D(RESOLUTION_WITH_BORDER * FACE_COUNT, RESOLUTION_WITH_BORDER * MIP_COUNT, RESOLUTION_WITH_BORDER, ResourceFormat::RGBA16Float, 1, nullptr,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
}

LightInjection::~LightInjection() {
    mpScene_ = nullptr;
    mpInjection_ = nullptr;
    mpDownSample_ = nullptr;
    mpRadius_ = nullptr;
}

LightInjection::SharedPtr LightInjection::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return LightInjection::SharedPtr(new LightInjection(pScene, dl));
}

void LightInjection::on_gui(Gui::Group& group) {

}

void LightInjection::on_execute(RenderContext* pContext, const Texture::SharedPtr& pShadowmap, const float4x4& shadowMatrix, const Texture::SharedPtr& pAlbedoTexture, const Texture::SharedPtr& pNormalTexture, const VoxelizationMeta& meta) {
    pContext->clearTexture(mpRadius_.get());
    mpInjection_->getVars()["CB"]["bufVoxelizationMeta"].setBlob(meta);
    mpInjection_->getVars()["CB"]["matShadowMatrix"] = shadowMatrix;
    mpInjection_->getVars()["texAlbedo"] = pAlbedoTexture;
    mpInjection_->getVars()["texNormal"] = pNormalTexture;
    mpInjection_->getVars()["texShadowMap"] = pShadowmap;
    mpInjection_->getVars()["texRadius"] = mpRadius_;
    mpInjection_->execute(pContext, meta.CellDim);
}

void LightInjection::do_down_sampler(RenderContext* pContext) {

}
