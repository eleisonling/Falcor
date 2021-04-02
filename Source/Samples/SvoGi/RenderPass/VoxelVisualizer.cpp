#include "VoxelVisualizer.h"

namespace {
    static std::string kDebugVolProg = "Samples/SvoGi/Shaders/VoxelizationVisualRaster.slang";
    static std::string kDebugSvoProg = "Samples/SvoGi/Shaders/VoxelizationVisualTracing.ps.slang";
    static std::string kDebugTraverseProg = "Samples/SvoGi/Shaders/VoxelizationVisualTraverse.ps.slang";
    static std::string kSamplerDefine = "USE_SAMPLER";

    enum class VisualType {
        VoxelRaster,
        VoxelTracing,
        SurfaceTraverse,

        MAX_COUNT,
    };

    const Gui::RadioButtonGroup kVisualTypeSelectButtons = {
        { (uint32_t)VisualType::VoxelRaster,        "VoxelRaster",      false },
        { (uint32_t)VisualType::VoxelTracing,       "VoxelTracing",     true },
        { (uint32_t)VisualType::SurfaceTraverse,    "SurfaceTraverse",  true }
    };


}

VoxelVisualizer::VoxelVisualizer(const Scene::SharedPtr& pScene, Program::DefineList& programDefines)
    : mpScene_(pScene) {

    if(mUseSampler_) programDefines.add(kSamplerDefine);
    create_visualize_shaders(programDefines);
    create_visualize_resources();
}

void VoxelVisualizer::create_visualize_shaders(const Program::DefineList& programDefines) {
    // visual raster
    {
        Program::Desc rasterVDesc;
        rasterVDesc.addShaderLibrary(kDebugVolProg).vsEntry("vs_main").psEntry("raster_brick");
        auto pDebugProg = GraphicsProgram::create(rasterVDesc, programDefines);
        mpVisualR_ = GraphicsState::create();
        mpVisualR_->setProgram(pDebugProg);
        mpVisualVarsR_ = GraphicsVars::create(pDebugProg.get());
    }


    // create render state
    RasterizerState::Desc rasterDesc{};
    rasterDesc.setFillMode(RasterizerState::FillMode::Wireframe).setCullMode(RasterizerState::CullMode::None);
    RasterizerState::SharedPtr rasterState = RasterizerState::create(rasterDesc);
    mpVisualR_->setRasterizerState(rasterState);


    BlendState::Desc blendDesc{};
    blendDesc.setRtBlend(0, true).setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::Zero);
    BlendState::SharedPtr blendState = BlendState::create(blendDesc);


    // visual tracing
    {
        Program::Desc dBrick;
        dBrick.addShaderLibrary(kDebugSvoProg).psEntry("brick_main");
        mpVisualTracing_ = FullScreenPass::create(dBrick, programDefines);
        mpVisualTracing_->getState()->setBlendState(blendState);
    }

    // surface traverse
    {
        mpRasterScene_ = RasterScenePass::create(mpScene_, kDebugTraverseProg, "", "ps_main", programDefines);
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

void VoxelVisualizer::do_visual_brick(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    mVoxelizationMeta_.CurLevel = mLevel_;

    switch ((VisualType)mVisualType_)
    {
    case VisualType::VoxelRaster:
    {
        uint32_t instanceCount = mVoxelizationMeta_.CellDim.x * mVoxelizationMeta_.CellDim.y * mVoxelizationMeta_.CellDim.z;
        mpVisualR_->setFbo(pDstFbo);
        mpVisualR_->setVao(mpRasterVao_);
        mpVisualVarsR_->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualVarsR_["texBrickTexValue"] = mpBrickAlbedoTexture_;
        mpVisualVarsR_["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpVisualVarsR_["bufSvoNodeColor"] = mpSVONodeColorBuffer_;
        mpVisualVarsR_["spTexture"] = mpSampler_;
        mpVisualVarsR_["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        pContext->drawIndexedInstanced(mpVisualR_.get(), mpVisualVarsR_.get(), (uint32_t)mpRasterMesh_->getIndices().size(), instanceCount, 0, 0, 0);
    }
        break;
    case VisualType::VoxelTracing:
    {
        mpVisualTracing_->getVars()->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpVisualTracing_["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        mpVisualTracing_["texBrickValue"] = mpBrickAlbedoTexture_;
        mpVisualTracing_["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpVisualTracing_["bufSvoNodeColor"] = mpSVONodeColorBuffer_;
        mpVisualTracing_["spTexSampler"] = mpSampler_;
        mpVisualTracing_["CB"]["fViewportDims"] = float2{ pDstFbo->getWidth(), pDstFbo->getHeight() };
        mpVisualTracing_->execute(pContext, pDstFbo);
    }
        break;
    case VisualType::SurfaceTraverse:
        mpRasterScene_->getVars()->setParameterBlock("gScene", mpScene_->getParameterBlock());
        mpRasterScene_->getVars()["texBrickTexValue"] = mpBrickAlbedoTexture_;
        mpRasterScene_->getVars()["bufSvoNodeNext"] = mpSVONodeNextBuffer_;
        mpRasterScene_->getVars()["bufSvoNodeColor"] = mpSVONodeColorBuffer_;
        mpRasterScene_->getVars()["spTexture"] = mpSampler_;
        mpRasterScene_->getVars()["CB"]["bufVoxelMeta"].setBlob(mVoxelizationMeta_);
        mpRasterScene_->renderScene(pContext, pDstFbo);
    default:
        break;
    }
}

VoxelVisualizer::~VoxelVisualizer() {
    mpScene_ = nullptr;
    mpBrickAlbedoTexture_ = nullptr;
    mpSampler_ = nullptr;
    mpSVONodeNextBuffer_ = nullptr;
    mpSVONodeColorBuffer_ = nullptr;
    mpVisualTracing_ = nullptr;
    mpVisualVarsR_ = nullptr;
    mpVisualR_ = nullptr;
    mpRasterMesh_ = nullptr;
    mpRasterVao_ = nullptr;
}

VoxelVisualizer::SharedPtr VoxelVisualizer::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());
    return VoxelVisualizer::SharedPtr(new VoxelVisualizer(pScene, dl));
}

void VoxelVisualizer::on_gui(Gui::Group& group) {
    group.var("Level", mLevel_, 1u, 8u);
    group.radioButtons(kVisualTypeSelectButtons, mVisualType_);

    if (group.checkbox("Use Sampler", mUseSampler_)) {
        if (mUseSampler_) {
            mpVisualTracing_->addDefine(kSamplerDefine);
            mpVisualR_->getProgram()->addDefine(kSamplerDefine);
            mpRasterScene_->getProgram()->addDefine(kSamplerDefine);
        }
        else {
            mpVisualTracing_->removeDefine(kSamplerDefine);
            mpVisualR_->getProgram()->removeDefine(kSamplerDefine);
            mpRasterScene_->getProgram()->removeDefine(kSamplerDefine);
        }
    }
}

void VoxelVisualizer::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("Visualize");
    do_visual_brick(pContext, pDstFbo);
}
