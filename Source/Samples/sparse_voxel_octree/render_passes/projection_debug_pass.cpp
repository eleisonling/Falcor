#include "projection_debug_pass.h"
#include "projection_debug_meta.slangh"

namespace {
    static std::string kDebugProg = "Samples/sparse_voxel_octree/render_passes/projection_debug.slang";
    const Gui::DropdownList kDimType = {
        { 0u, "YZ Space" },
        { 1u, "XZ Space" },
        { 2u, "XY Space" },
    };
}

projection_debug_pass::~projection_debug_pass() {
    mpScene_ = nullptr;
}

projection_debug_pass::SharedPtr projection_debug_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_debug;
    d_debug.addShaderLibrary(kDebugProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new projection_debug_pass(pScene, d_debug, dl));
}

void projection_debug_pass::debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    mpVars["CB"]["gProjectionMeta"]["Dim"] = orthDim_;
    mpState->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get());
}

void projection_debug_pass::on_gui_render(Gui::Group& group) {
    group.dropdown("Output Type", kDimType, orthDim_);
}

projection_debug_pass::projection_debug_pass(const Scene::SharedPtr& pScene, const Program::Desc& debugProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(debugProgDesc, programDefines)
    , mpScene_(pScene) {
    assert(mpScene_);
    mpVars["CB"]["gProjectionMeta"]["Min"] = mpScene_->getSceneBounds().minPoint;
    mpVars["CB"]["gProjectionMeta"]["Max"] = mpScene_->getSceneBounds().maxPoint;
}
