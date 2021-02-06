#include "volumetric_pass.h"
#include "voxel_meta.slangh"

namespace {
    static std::string kVolumetricProg = "Samples/sparse_voxel_octree/render_passes/volumetric.slang";
    static std::string kPixelAvgProg = "Samples/sparse_voxel_octree/render_passes/pixel_avg.cs.slang";
    static std::string kBuildSVOProg = "Samples/sparse_voxel_octree/render_passes/build_svo.cs.slang";
    static std::string kDebugVolProg = "Samples/sparse_voxel_octree/render_passes/debug_volumetric.slang";
    static voxel_meta kVoxelMeta{};
}

volumetric_pass::~volumetric_pass() {
    mpScene_ = nullptr;
    mpPackedPixelBuffer_ = nullptr;
    mpPixelAvgState_ = nullptr;
    mpPixelAvgVars_ = nullptr;

    mpSVONodeBuffer_ = nullptr;
    mpIndirectDrawArgs_ = nullptr;
    mpTagNode_ = nullptr;
    mpTagNodeVars_ = nullptr;
    mpDivideArg_ = nullptr;
    mpDivideArgVars_ = nullptr;
    mpDivideSubNode_ = nullptr;
    mpDivideSubNodeVars_ = nullptr;

    mpDebugVars_ = nullptr;
    mpDebugState_ = nullptr;
    mpDebugMesh_ = nullptr;
    mpDebugVao_ = nullptr;
}

volumetric_pass::SharedPtr volumetric_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_volumetric;
    d_volumetric.addShaderLibrary(kVolumetricProg)
        .vsEntry("")
        .gsEntry("gs_main")
        .psEntry("ps_main");

    Program::Desc d_debugVol;
    d_debugVol.addShaderLibrary(kDebugVolProg).vsEntry("vs_main").psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new volumetric_pass(pScene, d_volumetric, d_debugVol, dl));
}

void volumetric_pass::volumetric_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("do volumetric");

    // do clear
    pContext->clearUAV(mpPackedPixelBuffer_->getUAV().get(), float4(0, 0, 0 ,0));
    mpState->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);

    // avg result
    uint3 group = { (kVoxelMeta.CellDim.x * kVoxelMeta.CellDim.y * kVoxelMeta.CellDim.z + g_avgThreads - 1) / g_avgThreads, 1, 1 };
    pContext->dispatch(mpPixelAvgState_.get(), mpPixelAvgVars_.get(), group);

    build_svo(pContext, pDstFbo);

    needRefresh_ = false;
}

void volumetric_pass::build_svo(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {

    PROFILE("build svo");
    pContext->clearUAV(mpSVONodeBuffer_->getUAV().get(), uint4{ 0, 0, 0, 0 });

}

void volumetric_pass::debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("debug volumetric");
    uint32_t instanceCount = kVoxelMeta.CellDim.x * kVoxelMeta.CellDim.y * kVoxelMeta.CellDim.z;
    mpDebugState_->setFbo(pDstFbo);
    mpDebugState_->setVao(mpDebugVao_);
    mpDebugVars_->setParameterBlock("gScene", mpScene_->getParameterBlock());
    pContext->drawIndexedInstanced(mpDebugState_.get(), mpDebugVars_.get(), (uint32_t)mpDebugMesh_->getIndices().size(), instanceCount, 0, 0, 0);
}

void volumetric_pass::on_gui_render(Gui::Group& group) {
    rebuildBuffer_ |= group.var("Cell Size", cellSize_, .05f, 0.1f, 0.01f);
    if (group.button("Rebuild")) {
        if (rebuildBuffer_) {
            rebuild_pixel_data_buffers();
            rebuild_svo_buffers();
        }
        needRefresh_ = true;
    }
}

volumetric_pass::volumetric_pass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines)
    : BaseGraphicsPass(volumetricProgDesc, programDefines)
    , mpScene_(pScene) {

    assert(mpScene_);
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

        Program::Desc d_pixelAvg;
        d_pixelAvg.addShaderLibrary(kPixelAvgProg).csEntry("cs_avg");
        auto pAvgProg = ComputeProgram::create(d_pixelAvg, programDefines);
        mpPixelAvgState_ = ComputeState::create();
        mpPixelAvgState_->setProgram(pAvgProg);
        mpPixelAvgVars_ = ComputeVars::create(pAvgProg.get());
    }

    create_svo_shaders(programDefines);

    rebuild_debug_drawbuffers(debugVolProgDesc, programDefines);
    rebuild_pixel_data_buffers();
    rebuild_svo_buffers();
}

void volumetric_pass::rebuild_pixel_data_buffers() {

    rebuildBuffer_ = false;
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil((bound.extent() + cellSize_) / cellSize_);

    {
        size_t bufferSize = size_t(cellDim.x) * cellDim.y * cellDim.z * sizeof(uint32_t) * 5;
        if (mpPackedPixelBuffer_ && mpPackedPixelBuffer_->getSize() == bufferSize) return;
        mpPackedPixelBuffer_ = Buffer::create(bufferSize);
    }

    kVoxelMeta.CellDim = cellDim;
    kVoxelMeta.CellSize = cellSize_;
    kVoxelMeta.Min = bound.minPoint;

    mpVars["gPixelPacked"] = mpPackedPixelBuffer_;
    mpVars["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);

    mpPixelAvgVars_["gPixelPacked"] = mpPackedPixelBuffer_;
    mpPixelAvgVars_["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);

    mpDebugMesh_ = TriangleMesh::createCube(cellSize_);
    mpDebugVao_->getVertexBuffer(0)->setBlob(mpDebugMesh_->getVertices().data(), 0, sizeof(TriangleMesh::Vertex) * mpDebugMesh_->getVertices().size());
    mpDebugVao_->getIndexBuffer()->setBlob(mpDebugMesh_->getIndices().data(), 0, sizeof(uint32_t) * mpDebugMesh_->getIndices().size());
    mpDebugVars_["gPixelPacked"] = mpPackedPixelBuffer_;
    mpDebugVars_["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);
}

void volumetric_pass::create_svo_shaders(Program::DefineList& programDefines) {

    {
        Program::Desc d_tagNode;
        d_tagNode.addShaderLibrary(kBuildSVOProg).csEntry("tag_node");
        auto pProg = ComputeProgram::create(d_tagNode, programDefines);
        mpTagNode_ = ComputeState::create();
        mpTagNode_->setProgram(pProg);
        mpTagNodeVars_ = ComputeVars::create(pProg.get());
    }

    {
        Program::Desc d_divideArg;
        d_divideArg.addShaderLibrary(kBuildSVOProg).csEntry("caculate_divide_indirect_arg");
        auto pProg = ComputeProgram::create(d_divideArg, programDefines);
        mpDivideArg_ = ComputeState::create();
        mpDivideArg_->setProgram(pProg);
        mpDivideArgVars_ = ComputeVars::create(pProg.get());
    }

    {
        Program::Desc d_divideSubNode;
        d_divideSubNode.addShaderLibrary(kBuildSVOProg).csEntry("sub_divide_node");
        auto pProg = ComputeProgram::create(d_divideSubNode, programDefines);
        mpDivideSubNode_ = ComputeState::create();
        mpDivideSubNode_ ->setProgram(pProg);
        mpDivideSubNodeVars_ = ComputeVars::create(pProg.get());
    }
}

void volumetric_pass::rebuild_svo_buffers() {
    uint32_t maxDim = std::max(std::max(kVoxelMeta.CellDim.x, kVoxelMeta.CellDim.y), kVoxelMeta.CellDim.z);
    uint32_t maxLevel = (uint32_t)std::ceil(std::log2f((float)maxDim));
    maxDim = (uint32_t)std::pow(2, maxLevel);

    size_t bufferSize = 0;
    for (uint32_t i = 1; i <= maxLevel; ++i) {
        bufferSize += (size_t)std::powf(8, (float)i);
    }
    bufferSize *= sizeof(uint32_t);
    mpSVONodeBuffer_ = Buffer::create(bufferSize);

    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectDrawArgs_ = Buffer::create(7 * sizeof(uint32_t), ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, Buffer::CpuAccess::None, initialValue);
}

void volumetric_pass::rebuild_debug_drawbuffers(const Program::Desc& debugVolProgDesc, Program::DefineList& programDefines) {
    // create debug program
    auto pDebugProg = GraphicsProgram::create(debugVolProgDesc, programDefines);
    mpDebugState_ = GraphicsState::create();
    mpDebugState_->setProgram(pDebugProg);
    mpDebugVars_ = GraphicsVars::create(pDebugProg.get());

    // create render state
    RasterizerState::Desc rasterDesc{};
    rasterDesc.setFillMode(RasterizerState::FillMode::Wireframe)
        .setCullMode(RasterizerState::CullMode::None);
    RasterizerState::SharedPtr rasterState = RasterizerState::create(rasterDesc);
    mpDebugState_->setRasterizerState(rasterState);


    VertexBufferLayout::SharedPtr pBufferLayout = VertexBufferLayout::create();
    pBufferLayout->addElement("POSITION",   offsetof(TriangleMesh::Vertex, position),   ResourceFormat::RGB32Float, 1, 0);
    pBufferLayout->addElement("NORMAL",     offsetof(TriangleMesh::Vertex, normal),     ResourceFormat::RGB32Float, 1, 1);
    pBufferLayout->addElement("TEXCOORD",   offsetof(TriangleMesh::Vertex, texCoord),   ResourceFormat::RG32Float,  1, 2);
    VertexLayout::SharedPtr pVertexLayout = VertexLayout::create();
    pVertexLayout->addBufferLayout(0, pBufferLayout);

    mpDebugMesh_ = TriangleMesh::createCube(cellSize_);
    Buffer::SharedPtr pVB = Buffer::createStructured(sizeof(TriangleMesh::Vertex), (uint32_t)mpDebugMesh_->getVertices().size(), Resource::BindFlags::Vertex, Buffer::CpuAccess::None, mpDebugMesh_->getVertices().data(), false);
    Buffer::SharedPtr pIB = Buffer::create(sizeof(uint32_t) * mpDebugMesh_->getIndices().size(), Resource::BindFlags::Index, Buffer::CpuAccess::None, mpDebugMesh_->getIndices().data());

    mpDebugVao_ = Vao::create(Vao::Topology::TriangleList, pVertexLayout, { pVB }, pIB, ResourceFormat::R32Uint);
}
