#pragma once
#include "Falcor.h"

using namespace Falcor;

class volumetric_pass : public BaseGraphicsPass, public std::enable_shared_from_this<volumetric_pass> {
public:
    using SharedPtr = std::shared_ptr<volumetric_pass>;
    virtual ~volumetric_pass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void volumetric_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    void debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    void on_gui_render(Gui::Group& group);
    bool need_refresh() const { return needRefresh_; }

private:
    volumetric_pass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines);
    void rebuild_voxel_buffers();
    void rebuild_debug_drawbuffers(const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines);

    // pixel volumetric
    Scene::SharedPtr mpScene_ = nullptr;
    Buffer::SharedPtr mpPixelPacked_ = nullptr;
    ComputeState::SharedPtr mpPixelAvgState_ = nullptr;
    ComputeVars::SharedPtr mpPixelAvgVars_ = nullptr;

    // pixel volumetric vars
    bool needRefresh_ = true;
    bool rebuildBuffer_ = false;
    float cellSize_ = 0.1f;

    // sparse Oct-tree builder

    // volumetric debug
    GraphicsVars::SharedPtr mpDebugVars_ = nullptr;
    GraphicsState::SharedPtr mpDebugState_ = nullptr;
    TriangleMesh::SharedPtr mpDebugMesh_ = nullptr;
    Vao::SharedPtr mpDebugVao_ = nullptr;
};
