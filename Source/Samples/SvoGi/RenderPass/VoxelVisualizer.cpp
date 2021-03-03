#include "VoxelVisualizer.h"

namespace {
    static std::string kDebugVolProg = "Samples/SvoGi/Shaders/VoxelizationVisualRaster.slang";
    static std::string kDebugSvoProg = "Samples/SvoGi/Shaders/VoxelizationVisualTracing.ps.slang";
}

VoxelVisualizer::VoxelVisualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines)
    : mpScene_(pScene) {
    create_visualize_shaders(programDefines);
    create_visualize_resources();
}

void VoxelVisualizer::create_visualize_shaders(const Program::DefineList& programDefines) {
    // visual raster
    {
        Program::Desc d_visualRaster;
        d_visualRaster.addShaderLibrary(kDebugVolProg).vsEntry("vs_main").psEntry("ps_main");
        // create debug program
        auto pDebugProg = GraphicsProgram::create(d_visualRaster, programDefines);
        mpVisualRaster_ = GraphicsState::create();
        mpVisualRaster_->setProgram(pDebugProg);
        mpVisualRasterVars_ = GraphicsVars::create(pDebugProg.get());

        // create render state
        RasterizerState::Desc rasterDesc{};
        rasterDesc.setFillMode(RasterizerState::FillMode::Wireframe)
            .setCullMode(RasterizerState::CullMode::None);
        RasterizerState::SharedPtr rasterState = RasterizerState::create(rasterDesc);
        mpVisualRaster_->setRasterizerState(rasterState);

    }

    // visual tracing
    {
        mpVisualTracing_ = FullScreenPass::create(kDebugSvoProg, programDefines);
    }
}

void VoxelVisualizer::create_visualize_resources() {

    VertexBufferLayout::SharedPtr pBufferLayout = VertexBufferLayout::create();
    pBufferLayout->addElement("POSITION", offsetof(TriangleMesh::Vertex, position), ResourceFormat::RGB32Float, 1, 0);
    pBufferLayout->addElement("NORMAL", offsetof(TriangleMesh::Vertex, normal), ResourceFormat::RGB32Float, 1, 1);
    pBufferLayout->addElement("TEXCOORD", offsetof(TriangleMesh::Vertex, texCoord), ResourceFormat::RG32Float, 1, 2);
    VertexLayout::SharedPtr pVertexLayout = VertexLayout::create();
    pVertexLayout->addBufferLayout(0, pBufferLayout);

    mpRasterMesh_ = TriangleMesh::createCube(1.0f);
    Buffer::SharedPtr pVB = Buffer::createStructured(sizeof(TriangleMesh::Vertex), (uint32_t)mpRasterMesh_->getVertices().size(), Resource::BindFlags::Vertex, Buffer::CpuAccess::None, mpRasterMesh_->getVertices().data(), false);
    Buffer::SharedPtr pIB = Buffer::create(sizeof(uint32_t) * mpRasterMesh_->getIndices().size(), Resource::BindFlags::Index, Buffer::CpuAccess::None, mpRasterMesh_->getIndices().data());
    mpRasterVao_ = Vao::create(Vao::Topology::TriangleList, pVertexLayout, { pVB }, pIB, ResourceFormat::R32Uint);
    mpRasterVao_->getVertexBuffer(0)->setBlob(mpRasterMesh_->getVertices().data(), 0, sizeof(TriangleMesh::Vertex) * mpRasterMesh_->getVertices().size());
    mpRasterVao_->getIndexBuffer()->setBlob(mpRasterMesh_->getIndices().data(), 0, sizeof(uint32_t) * mpRasterMesh_->getIndices().size());
}

VoxelVisualizer::~VoxelVisualizer() {
    mpScene_ = nullptr;
    mpVisualTexture_ = nullptr;
    mpSVONodeNextBuffer_ = nullptr;
    mpVisualTracing_ = nullptr;
    mpVisualRasterVars_ = nullptr;
    mpVisualRaster_ = nullptr;
    mpRasterMesh_ = nullptr;
    mpRasterVao_ = nullptr;
}

VoxelVisualizer::SharedPtr VoxelVisualizer::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());
    return VoxelVisualizer::SharedPtr(new VoxelVisualizer(pScene, dl));
}

void VoxelVisualizer::on_gui(Gui::Group& group) {
    group.checkbox("Use Tracing Method", mDebugSVOTracing_);
}

void VoxelVisualizer::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& pTexSampler) {
    PROFILE("debug volumetric");

    if (mDebugSVOTracing_) {
        mpVisualTracing_->getVars()->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualTracing_->getVars()["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        mpVisualTracing_->getVars()["texPackedAlbedo"] = mpVisualTexture_;
        mpVisualTracing_->getVars()["bufSvoNode"] = mpSVONodeNextBuffer_;
        mpVisualTracing_->getVars()["spTexSampler"] = pTexSampler;
        mpVisualTracing_->getVars()["CB"]["fViewportDims"] = float2{ pDstFbo->getWidth(), pDstFbo->getHeight() };
        mpVisualTracing_->execute(pContext, pDstFbo);
    } else {
        uint32_t instanceCount = mVoxelizationMeta_.CellDim.x * mVoxelizationMeta_.CellDim.y * mVoxelizationMeta_.CellDim.z;
        mpVisualRaster_->setFbo(pDstFbo);
        mpVisualRaster_->setVao(mpRasterVao_);
        mpVisualRasterVars_->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualRasterVars_["texPackedAlbedo"] = mpVisualTexture_;
        mpVisualRasterVars_["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        mpVisualRasterVars_["spTexSampler"] = pTexSampler;
        pContext->drawIndexedInstanced(mpVisualRaster_.get(), mpVisualRasterVars_.get(), (uint32_t)mpRasterMesh_->getIndices().size(), instanceCount, 0, 0, 0);
    }
}
