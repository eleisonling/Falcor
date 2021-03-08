#include "VoxelVisualizer.h"

namespace {
    static std::string kDebugVolProg = "Samples/SvoGi/Shaders/VoxelizationVisualRaster.slang";
    static std::string kDebugSvoProg = "Samples/SvoGi/Shaders/VoxelizationVisualTracing.ps.slang";
    static std::string kSamplerDefine = "USE_SAMPLER";

    enum VisualType {
        VisualVoxel,
        VisualBrick,

        TypeNum
    };


    const Gui::DropdownList kVisualType = {
        { (uint32_t)VisualType::VisualVoxel, "Voxel" },
        { (uint32_t)VisualType::VisualBrick, "Brick" },
    };

}

VoxelVisualizer::VoxelVisualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines)
    : mpScene_(pScene) {
    create_visualize_shaders(programDefines);
    create_visualize_resources();
}

void VoxelVisualizer::create_visualize_shaders(const Program::DefineList& programDefines) {
    // visual raster
    {
        Program::Desc rasterVDesc;
        rasterVDesc.addShaderLibrary(kDebugVolProg).vsEntry("vs_main").psEntry("ps_main");
        auto pDebugProg = GraphicsProgram::create(rasterVDesc, programDefines);
        mpVisualR_[VisualVoxel] = GraphicsState::create();
        mpVisualR_[VisualVoxel]->setProgram(pDebugProg);
        mpVisualVarsR_[VisualVoxel] = GraphicsVars::create(pDebugProg.get());
    }

    {
        Program::Desc rasterVDesc;
        rasterVDesc.addShaderLibrary(kDebugVolProg).vsEntry("vs_main").psEntry("raster_brick");
        auto pDebugProg = GraphicsProgram::create(rasterVDesc, programDefines);
        mpVisualR_[VisualBrick] = GraphicsState::create();
        mpVisualR_[VisualBrick]->setProgram(pDebugProg);
        mpVisualVarsR_[VisualBrick] = GraphicsVars::create(pDebugProg.get());
    }


    // create render state
    RasterizerState::Desc rasterDesc{};
    rasterDesc.setFillMode(RasterizerState::FillMode::Wireframe).setCullMode(RasterizerState::CullMode::None);
    RasterizerState::SharedPtr rasterState = RasterizerState::create(rasterDesc);
    mpVisualR_[VisualVoxel]->setRasterizerState(rasterState);
    mpVisualR_[VisualBrick]->setRasterizerState(rasterState);


    // visual tracing
    {
        Program::Desc dVoxel;
        dVoxel.addShaderLibrary(kDebugSvoProg).psEntry("voxel_main");
        mpVisualTracing_ = FullScreenPass::create(dVoxel, programDefines);
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

void VoxelVisualizer::do_visual_voxel(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    if (mUseTacing_) {
        mpVisualTracing_->getVars()->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualTracing_->getVars()["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        mpVisualTracing_->getVars()["texVoxelValue"] = mpVoxelTexture_;
        mpVisualTracing_->getVars()["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpVisualTracing_->getVars()["spTexSampler"] = mpSampler_;
        mpVisualTracing_->getVars()["CB"]["fViewportDims"] = float2{ pDstFbo->getWidth(), pDstFbo->getHeight() };
        mpVisualTracing_->execute(pContext, pDstFbo);
    }
    else {
        uint32_t instanceCount = mVoxelizationMeta_.CellDim.x * mVoxelizationMeta_.CellDim.y * mVoxelizationMeta_.CellDim.z;
        mpVisualR_[VisualVoxel]->setFbo(pDstFbo);
        mpVisualR_[VisualVoxel]->setVao(mpRasterVao_);
        mpVisualVarsR_[VisualVoxel]->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualVarsR_[VisualVoxel]["texPackedAlbedo"] = mpVoxelTexture_;
        mpVisualVarsR_[VisualVoxel]["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpVisualVarsR_[VisualVoxel]["spTexture"] = mpSampler_;
        mpVisualVarsR_[VisualVoxel]["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        pContext->drawIndexedInstanced(mpVisualR_[VisualVoxel].get(), mpVisualVarsR_[VisualVoxel].get(), (uint32_t)mpRasterMesh_->getIndices().size(), instanceCount, 0, 0, 0);
    }
}

void VoxelVisualizer::do_visual_brick(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    if (mUseTacing_) {
    }
    else {
        uint32_t instanceCount = mVoxelizationMeta_.CellDim.x * mVoxelizationMeta_.CellDim.y * mVoxelizationMeta_.CellDim.z;
        mpVisualR_[VisualBrick]->setFbo(pDstFbo);
        mpVisualR_[VisualBrick]->setVao(mpRasterVao_);
        mpVisualVarsR_[VisualBrick]->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualVarsR_[VisualBrick]["texBrickTexValue"] = mpBrickAlbedoTexture_;
        mpVisualVarsR_[VisualBrick]["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpVisualVarsR_[VisualBrick]["bufSvoNodeColor"] = mpSVONodeColorBuffer_;
        mpVisualVarsR_[VisualBrick]["spTexture"] = mpSampler_;
        mpVisualVarsR_[VisualBrick]["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        pContext->drawIndexedInstanced(mpVisualR_[VisualBrick].get(), mpVisualVarsR_[VisualBrick].get(), (uint32_t)mpRasterMesh_->getIndices().size(), instanceCount, 0, 0, 0);
    }
}

VoxelVisualizer::~VoxelVisualizer() {
    mpScene_ = nullptr;
    mpVoxelTexture_ = nullptr;
    mpBrickAlbedoTexture_ = nullptr;
    mpSampler_ = nullptr;
    mpSVONodeNextBuffer_ = nullptr;
    mpSVONodeColorBuffer_ = nullptr;
    mpVisualTracing_ = nullptr;
    mpVisualVarsR_[VisualVoxel] = nullptr;
    mpVisualVarsR_[VisualBrick] = nullptr;
    mpVisualR_[VisualVoxel] = nullptr;
    mpVisualR_[VisualBrick] = nullptr;
    mpRasterMesh_ = nullptr;
    mpRasterVao_ = nullptr;
}

VoxelVisualizer::SharedPtr VoxelVisualizer::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());
    dl.add(kSamplerDefine);
    return VoxelVisualizer::SharedPtr(new VoxelVisualizer(pScene, dl));
}

void VoxelVisualizer::on_gui(Gui::Group& group) {
    group.checkbox("Use Tracing Method", mUseTacing_);

    if (group.checkbox("Use Sampler", mUseSampler_)) {
        if (mUseSampler_) {
            mpVisualTracing_->addDefine(kSamplerDefine);
            mpVisualR_[VisualVoxel]->getProgram()->addDefine(kSamplerDefine);
            mpVisualR_[VisualBrick]->getProgram()->addDefine(kSamplerDefine);
        }
        else {
            mpVisualTracing_->removeDefine(kSamplerDefine);
            mpVisualR_[VisualVoxel]->getProgram()->removeDefine(kSamplerDefine);
            mpVisualR_[VisualBrick]->getProgram()->removeDefine(kSamplerDefine);
        }
    }

    group.dropdown("Visual Type", kVisualType, mType_);
}

void VoxelVisualizer::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("Visualize");

    switch (mType_)
    {
    case VisualType::VisualVoxel:
        do_visual_voxel(pContext, pDstFbo);
        break;
    case VisualType::VisualBrick:
        do_visual_brick(pContext, pDstFbo);
        break;
    case VisualType::TypeNum:
        break;
    default:
        break;
    }
}
