#include "VoxelizationPass.h"

namespace {
    static std::string kClearProg = "Samples/SvoGi/Shaders/VoxelizationClear.cs.slang";
    static std::string kIndirectArgProg = "Samples/SvoGi/Shaders/VoxelizationDispatch.cs.slang";
    static std::string kVolumetricProg = "Samples/SvoGi/Shaders/Voxelization.slang";
    static std::string kBuildSVOProg = "Samples/SvoGi/Shaders/VoxelizationSvo.cs.slang";
    static VoxelizationMeta kVoxelizationMeta{};
}

VoxelizationPass::~VoxelizationPass() {
    mpScene_ = nullptr;
    mpViewProjections_ = nullptr;
    mpPackedAlbedo_ = nullptr;
    mpPackedNormal_ = nullptr;
    mpFragPositions_ = nullptr;

    mpClearTexture3D_ = nullptr;
    mpClearBuffer1D_ = nullptr;
    mpSVONodeBuffer_ = nullptr;
    mpIndirectArgBuffer_ = nullptr;
    mpTagNode_ = nullptr;
    mpCaculateIndirectArg_ = nullptr;
    mpCaculateIndirectArgVars_ = nullptr;
    mpDivideSubNode_ = nullptr;
    mpDivideSubNodeVars_ = nullptr;
    mpAtomicAndIndirect_ = nullptr;
}

VoxelizationPass::SharedPtr VoxelizationPass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_volumetric;
    d_volumetric.addShaderLibrary(kVolumetricProg)
        .vsEntry("")
        .gsEntry("gs_main")
        .psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new VoxelizationPass(pScene, d_volumetric, dl));
}

void VoxelizationPass::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("voxelization");

    // do clear
    do_clear(pContext);

    mpState->setFbo(pDstFbo);
    mpVars["texPackedAlbedo"] = mpPackedAlbedo_;
    mpVars["texPackedNormal"] = mpPackedNormal_;
    mpVars["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
    mpVars["bufFragPosition"] = mpFragPositions_;
    mpVars["CB"]["matViewProjections"] = mpViewProjections_;
    mpVars["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    {
        mpFragPosIndirectArg_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
        mpFragPosIndirectArg_->execute(pContext, uint3(1));
    }

    uint32_t* data = (uint32_t*)mpAtomicAndIndirect_->map(Buffer::MapType::Read);
    mpAtomicAndIndirect_->unmap();


    do_build_svo(pContext);
    mNeedRefresh_ = false;
}

void VoxelizationPass::do_build_svo(RenderContext* pContext) {

    PROFILE("build svo");
    pContext->clearUAV(mpSVONodeBuffer_->getUAV().get(), uint4{ 0, 0, 0, 0 });
    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_->setBlob(initialValue, 0, sizeof(uint32_t) * 7);

    for (uint32_t i = 1; i <= kVoxelizationMeta.TotalLevel; ++i) {
        // tag
        kVoxelizationMeta.CurLevel = i;
        // bound resource to shader
        mpTagNode_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpTagNode_["texPackedAlbedo"] = mpPackedAlbedo_;
        mpTagNode_["bufSvoNode"] = mpSVONodeBuffer_;
        mpTagNode_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
        mpTagNode_["bufFragPosition"] = mpFragPositions_;
        mpTagNode_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpTagNode_->executeIndirect(pContext, mpAtomicAndIndirect_.get(), FRAG_NEXT_INDIRECT * 4);

        if (i < kVoxelizationMeta.TotalLevel) {
            // calculate indirect
            mpCaculateIndirectArgVars_["bufDivideIndirectArg"] = mpIndirectArgBuffer_;
            pContext->dispatch(mpCaculateIndirectArg_.get(), mpCaculateIndirectArgVars_.get(), uint3{ 1 ,1 ,1 });

            // sub-divide
            mpDivideSubNodeVars_["bufDivideIndirectArg"] = mpIndirectArgBuffer_;
            mpDivideSubNodeVars_["bufSvoNode"] = mpSVONodeBuffer_;
            pContext->dispatchIndirect(mpDivideSubNode_.get(), mpDivideSubNodeVars_.get(), mpIndirectArgBuffer_.get(), 0);
        }
    }
}

void VoxelizationPass::on_gui(Gui::Group& group) {}

const VoxelizationMeta& VoxelizationPass::get_voxelization_meta() const {
    return kVoxelizationMeta;
}

VoxelizationPass::VoxelizationPass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, Program::DefineList& programDefines)
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
    }

    do_create_vps();
    do_create_shaders(programDefines);
    do_rebuild_pixel_data_buffers();
    do_rebuild_svo_buffers();
}

void VoxelizationPass::do_rebuild_pixel_data_buffers() {

    auto& bound = mpScene_->getSceneBounds();
    uint3 cellDim = uint3(mVoxelGridResolution_);
    {
        size_t bufferSize = size_t(cellDim.x) * cellDim.y * cellDim.z * sizeof(uint32_t);
        if (!mpPackedAlbedo_ || mpPackedAlbedo_->getWidth() != cellDim.x ||  mpPackedAlbedo_->getHeight() != cellDim.y || mpPackedAlbedo_->getDepth() != cellDim.z) {
            mpPackedAlbedo_ = Texture::create3D(cellDim.x, cellDim.y, cellDim.z, ResourceFormat::RGBA8Unorm, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
        }

        if (!mpPackedNormal_ || mpPackedNormal_->getWidth() != cellDim.x ||  mpPackedNormal_->getHeight() != cellDim.y || mpPackedNormal_->getDepth() != cellDim.z) {
            mpPackedNormal_ = Texture::create3D(cellDim.x, cellDim.y, cellDim.z, ResourceFormat::RGBA8Unorm, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
        }
    }

    mpFragPositions_ = Buffer::create(3U * mVoxelGridResolution_ * mVoxelGridResolution_ * mVoxelGridResolution_ * sizeof(uint32_t));

    kVoxelizationMeta.CellDim = cellDim;
    kVoxelizationMeta.Min = bound.minPoint;
    kVoxelizationMeta.Max = bound.maxPoint;
    kVoxelizationMeta.CellNum = kVoxelizationMeta.CellDim.x * kVoxelizationMeta.CellDim.y * kVoxelizationMeta.CellDim.z;
    kVoxelizationMeta.TotalLevel = (uint32_t)std::ceil(std::log2f((float)mVoxelGridResolution_));
    assert(kVoxelizationMeta.TotalLevel <= MAX_LEVEL);
  }


void VoxelizationPass::do_create_shaders(Program::DefineList& programDefines) {

    // clear
    {
        mpClearTexture3D_ = ComputePass::create(kClearProg, "clear_texture_3d", programDefines);
        mpClearBuffer1D_ = ComputePass::create(kClearProg, "clear_buffer_linear", programDefines);
    }

    {
        mpFragPosIndirectArg_ = ComputePass::create(kIndirectArgProg, "make_indirect_frag_pos_arg", programDefines);
        mpNodeIndirect_ = ComputePass::create(kIndirectArgProg, "make_indirect_frag_node_arg", programDefines);
    }

    {
        mpTagNode_ = ComputePass::create(kBuildSVOProg, "tag_node", programDefines);
    }

    {
        Program::Desc d_divideArg;
        d_divideArg.addShaderLibrary(kBuildSVOProg).csEntry("caculate_divide_indirect_arg");
        auto pProg = ComputeProgram::create(d_divideArg, programDefines);
        mpCaculateIndirectArg_ = ComputeState::create();
        mpCaculateIndirectArg_->setProgram(pProg);
        mpCaculateIndirectArgVars_ = ComputeVars::create(pProg.get());
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

void VoxelizationPass::do_create_vps() {
    mpViewProjections_ = Buffer::createStructured(sizeof(float4x4), 3);

    auto& bounds = mpScene_->getSceneBounds();
    float radius = bounds.radius();
    float3 center = bounds.center();

    float4x4 proj[3] = {};
    proj[0] = glm::ortho(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
    proj[1] = glm::ortho(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
    proj[2] = glm::ortho(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);

    float4x4 viewProj[3] = {};
    viewProj[0] = proj[0] * glm::lookAt(float3(center.x + radius * 1.5f, center.y, center.z), center, float3(0, 1, 0));
    viewProj[1] = proj[1] * glm::lookAt(float3(center.x, center.y + radius * 1.5f, center.z), center, float3(1, 0, 0));
    viewProj[2] = proj[2] * glm::lookAt(float3(center.x, center.y, center.z + radius * 1.5f), center, float3(0, 1, 0));

    mpViewProjections_->setBlob(viewProj, 0, sizeof(float4x4) * 3);
}

void VoxelizationPass::do_rebuild_svo_buffers() {

    mpAtomicAndIndirect_ = Buffer::create(sizeof(uint32_t) * BUFFER_COUNT);

    mSVONodeNum_ = 0;
    for (uint32_t i = 1; i <= kVoxelizationMeta.TotalLevel; ++i) {
        uint32_t levelNum = (uint32_t)std::powf(8, (float)i);
        mSVONodeNum_ += levelNum;
        mSVOPerLevelNodeNum_.push_back(levelNum);
    }
    mpSVONodeBuffer_ = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));

    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_ = Buffer::create(7 * sizeof(uint32_t), ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, Buffer::CpuAccess::None, initialValue);
}

void VoxelizationPass::do_clear(RenderContext* pContext) {
    PROFILE("clear");
    mpClearTexture3D_->getVars()["texClear"] = mpPackedAlbedo_;
    mpClearTexture3D_->getVars()["CB"]["uDim3"] = uint3(mVoxelGridResolution_);
    mpClearTexture3D_->execute(pContext, uint3(mVoxelGridResolution_));
    mpClearTexture3D_->getVars()["texClear"] = mpPackedNormal_;
    mpClearTexture3D_->execute(pContext, uint3(mVoxelGridResolution_));

    uint3 threads = extract_index_form_linear(mSVONodeNum_, uint3(DIVIDE_THREAD_SIZE));
    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONodeBuffer_;
    mpClearBuffer1D_->getVars()["CB"]["uDim3"] = uint3(DIVIDE_THREAD_SIZE);
    mpClearBuffer1D_->getVars()["CB"]["uDim1"] = mSVONodeNum_;
    mpClearBuffer1D_->execute(pContext, threads);

    threads = extract_index_form_linear(3 * mVoxelGridResolution_ * mVoxelGridResolution_ * mVoxelGridResolution_, uint3(DIVIDE_THREAD_SIZE));
    mpClearBuffer1D_->getVars()["bufClear"] = mpFragPositions_;
    mpClearBuffer1D_->getVars()["CB"]["uDim1"] = 3 * mVoxelGridResolution_ * mVoxelGridResolution_ * mVoxelGridResolution_;
    mpClearBuffer1D_->execute(pContext, threads);

    uint32_t initialVal[BUFFER_COUNT] = { 0 };
    mpAtomicAndIndirect_->setBlob(initialVal, 0, sizeof(uint32_t) * BUFFER_COUNT);
}
