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
    void rebuild_pixel_data_buffers();
    void create_svo_shaders(Program::DefineList& programDefines);
    void rebuild_svo_buffers();
    void rebuild_debug_vol_resources(const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines);
    void build_svo(RenderContext* pContext);
    /// <summary>
    /// because svo need pow(2,n) size, for the full usage, we must modify the cellSize
    /// </summary>
    void fixture_cell_size();

    // pixel volumetric
    Scene::SharedPtr mpScene_ = nullptr;
    Buffer::SharedPtr mpPackedPixelBuffer_ = nullptr;
    ComputeState::SharedPtr mpPixelAvgState_ = nullptr;
    ComputeVars::SharedPtr mpPixelAvgVars_ = nullptr;

    // pixel volumetric vars
    bool needRefresh_ = true;
    bool rebuildBuffer_ = false;
    float cellSize_ = 10.0f;

    // sparse Oct-tree builder
    Buffer::SharedPtr mpSVONodeBuffer_ = nullptr;
    Buffer::SharedPtr mpIndirectArgBuffer_ = nullptr;

    ComputeState::SharedPtr mpTagNode_ = nullptr;
    ComputeVars::SharedPtr mpTagNodeVars_ = nullptr;
    ComputeState::SharedPtr mpCaculateIndirectArg_ = nullptr;
    ComputeVars::SharedPtr mpCaculateIndirectArgVars_ = nullptr;
    ComputeState::SharedPtr mpDivideSubNode_ = nullptr;
    ComputeVars::SharedPtr mpDivideSubNodeVars_ = nullptr;

    // debug tracing svo
    FullScreenPass::SharedPtr mpTracingSvo_ = nullptr;
    bool debugSVOTracing_ = false;
    Buffer::SharedPtr mpSvoDebugTracingData_ = nullptr;

    // volumetric debug
    GraphicsVars::SharedPtr mpDebugVars_ = nullptr;
    GraphicsState::SharedPtr mpDebugState_ = nullptr;
    TriangleMesh::SharedPtr mpDebugMesh_ = nullptr;
    Vao::SharedPtr mpDebugVao_ = nullptr;
};
