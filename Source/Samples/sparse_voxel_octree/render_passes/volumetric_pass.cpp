#include "volumetric_pass.h"
#include "voxel_meta.slangh"

namespace {
    static std::string kVolumetricProg = "Samples/sparse_voxel_octree/render_passes/volumetric.slang";
    static std::string kDebugVolProg = "Samples/sparse_voxel_octree/render_passes/debug_volumetric.slang";
    static voxel_meta kVoxelMeta{};
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
    mpState->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    needRefresh_ = false;
}

void volumetric_pass::debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    mpDebugState_->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpDebugState_.get(), mpDebugVars_.get());
}

void volumetric_pass::on_gui_render(Gui::Group& group) {
    rebuildBuffer_ = group.var("Cell Size", cellSize_, .5f, 1.0f, 0.1f);
    if (group.button("Rebuild")) {
        if (rebuildBuffer_) {
            rebuild_buffer();
        }
        needRefresh_ = true;
    }
}

volumetric_pass::volumetric_pass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(volumetricProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);
    rebuild_buffer();

    {
        // setup volumetric states
        RasterizerState::Desc rasterDesc{};
        rasterDesc.setFillMode(RasterizerState::FillMode::Solid)
            .setCullMode(RasterizerState::CullMode::None)
            .setConservativeRasterization(true);
        RasterizerState::SharedPtr rasterState = RasterizerState::create(rasterDesc);
        mpState->setRasterizerState(rasterState);

        DepthStencilState::Desc dsDesc{};
        dsDesc.setDepthEnabled(false);
        DepthStencilState::SharedPtr dsState = DepthStencilState::create(dsDesc);
        mpState->setDepthStencilState(dsState);

        BlendState::Desc blendDesc{};
        blendDesc.setRenderTargetWriteMask(0, false, false, false, false);
        BlendState::SharedPtr blendState = BlendState::create(blendDesc);
        mpState->setBlendState(blendState);
    }

    // create debug program
    auto pDebugProg = GraphicsProgram::create(debugVolProgDesc, programDefines);
    mpDebugState_ = GraphicsState::create();
    mpDebugState_->setProgram(pDebugProg);
    mpDebugVars_ = GraphicsVars::create(pDebugProg.get());
}

void volumetric_pass::rebuild_buffer() {

    rebuildBuffer_ = false;
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil(bound.extent() / cellSize_);

    size_t bufferSize = size_t(cellDim.x) * cellDim.y * cellDim.z * sizeof(uint32_t);
    if (mpVoxelBuf_ && mpVoxelBuf_->getSize() == bufferSize) return;

    mpVoxelBuf_ = Buffer::create(bufferSize);
    void* data = mpVoxelBuf_->map(Buffer::MapType::WriteDiscard);
    memset(data, 0, bufferSize);
    mpVoxelBuf_->unmap();

    kVoxelMeta.CellDim = cellDim;
    kVoxelMeta.CellSize = cellSize_;
    kVoxelMeta.Min = bound.minPoint;

    mpVars["gVoxelColor"] = mpVoxelBuf_;
    mpVars["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);

    mpDebugVars_["gVoxelColor"] = mpVoxelBuf_;
    mpDebugVars_["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);
}
