#include "volumetric_pass.h"

namespace {
    static std::string kVolumetricProg = "Samples/sparse_voxel_octree/render_passes/volumetric.slang";
    static std::string kDebugVolProg = "Samples/sparse_voxel_octree/render_passes/debug_volumetric.slang";
}

volumetric_pass::~volumetric_pass() {
    mpScene_ = nullptr;
    mpDebugVars_ = nullptr;
    mpDebugState_ = nullptr;
}

volumetric_pass::SharedPtr volumetric_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_volumetric;
    d_volumetric.addShaderLibrary(kVolumetricProg).vsEntry("").gsEntry("gs_main").psEntry("ps_main");

    Program::Desc d_debugVol;
    d_debugVol.addShaderLibrary(kDebugVolProg).vsEntry("").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new volumetric_pass(pScene, d_volumetric, d_debugVol, dl));
}

void volumetric_pass::volumetric_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
}

void volumetric_pass::debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {

}

volumetric_pass::volumetric_pass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(volumetricProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);
    auto pDebugProg = GraphicsProgram::create(debugVolProgDesc, programDefines);
    mpDebugState_ = GraphicsState::create();
    mpDebugState_->setProgram(pDebugProg);
    mpDebugVars_ = GraphicsVars::create(pDebugProg.get());
}
