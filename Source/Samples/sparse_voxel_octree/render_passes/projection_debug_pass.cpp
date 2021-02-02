#include "projection_debug_pass.h"

namespace {
    static std::string kDebugProg = "Samples/sparse_voxel_octree/render_passes/debug_projection.slang";
}

debug_projection_pass::~debug_projection_pass() {
    mpScene_ = nullptr;
}

debug_projection_pass::SharedPtr debug_projection_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_debug;
    d_debug.addShaderLibrary(kDebugProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new debug_projection_pass(pScene, d_debug, dl));
}

void debug_projection_pass::debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    mpState->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get());
}

debug_projection_pass::debug_projection_pass(const Scene::SharedPtr& pScene, const Program::Desc& debugProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(debugProgDesc, programDefines)
    , mpScene_(pScene) {
    assert(mpScene_);
}
