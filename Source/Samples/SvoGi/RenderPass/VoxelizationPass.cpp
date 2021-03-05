#include "VoxelizationPass.h"

namespace {
    static std::string kClearProg = "Samples/SvoGi/Shaders/VoxelizationClear.cs.slang";
    static std::string kIndirectArgProg = "Samples/SvoGi/Shaders/VoxelizationDispatch.cs.slang";
    static std::string kVolumetricProg = "Samples/SvoGi/Shaders/Voxelization.slang";
    static std::string kBuildSVOProg = "Samples/SvoGi/Shaders/VoxelizationSvo.cs.slang";
    static std::string kBuildBrickProg = "Samples/SvoGi/Shaders/VoxelizationBrick.cs.slang";
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
    mpSVONodeBufferNext_ = nullptr;
    mpSVONodeBufferColor_ = nullptr;
    mpIndirectArgBuffer_ = nullptr;
    mpLevelAddressBuffer_ = nullptr;
    mpSVONeighbourBuffer_[AXIS_X] = nullptr;
    mpSVONeighbourBuffer_[AXIS_Y] = nullptr;
    mpSVONeighbourBuffer_[AXIS_Z] = nullptr;
    mpTagNode_ = nullptr;
    mpCaculateIndirectArg_ = nullptr;
    mpCaculateIndirectArgVars_ = nullptr;
    mpDivideSubNode_ = nullptr;
    mpAtomicAndIndirect_ = nullptr;
    mpBuildNeighbours_ = nullptr;

    mpAllocBrick_ = nullptr;
    mpWriteLeaf_ = nullptr;
    mpBorderTransfer_ = nullptr;
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

#ifdef _DEBUG
    uint32_t* data = (uint32_t*)mpAtomicAndIndirect_->map(Buffer::MapType::Read);
    assert(data[ATOM_FRAG_NEXT] < mpFragPositions_->getSize() / sizeof(uint32_t));
    mpAtomicAndIndirect_->unmap();
#endif


    do_build_svo(pContext);
    do_build_brick(pContext);
    mNeedRefresh_ = false;
}

void VoxelizationPass::do_build_svo(RenderContext* pContext) {

    PROFILE("BuildSvo");

    for (uint32_t i = 0; i < kVoxelizationMeta.TotalLevel; ++i) {
        {
            PROFILE("Tag");
            kVoxelizationMeta.CurLevel = i;
            mpTagNode_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
            mpTagNode_["bufSvoNode"] = mpSVONodeBufferNext_;
            mpTagNode_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
            mpTagNode_["bufFragPosition"] = mpFragPositions_;
            mpTagNode_->executeIndirect(pContext, mpAtomicAndIndirect_.get(), FRAG_NEXT_INDIRECT * 4);
        }

        // calculate indirect
        if (i > 0) {

            {
                PROFILE("Build Neighbour");
                mpBuildNeighbours_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
                mpBuildNeighbours_["bufSvoNode"] = mpSVONodeBufferNext_;
                mpBuildNeighbours_["bufNeighbour_X"] = mpSVONeighbourBuffer_[AXIS_X];
                mpBuildNeighbours_["bufNeighbour_Y"] = mpSVONeighbourBuffer_[AXIS_Y];
                mpBuildNeighbours_["bufNeighbour_Z"] = mpSVONeighbourBuffer_[AXIS_Z];
                mpBuildNeighbours_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
                mpBuildNeighbours_["bufFragPosition"] = mpFragPositions_;
                mpBuildNeighbours_->executeIndirect(pContext, mpAtomicAndIndirect_.get(), FRAG_NEXT_INDIRECT * 4);
            }
            {
                mpCaculateIndirectArgVars_["bufDivideIndirectArg"] = mpIndirectArgBuffer_;
                pContext->dispatch(mpCaculateIndirectArg_.get(), mpCaculateIndirectArgVars_.get(), uint3{ 1 ,1 ,1 });
            }

        }

        {
            PROFILE("Allocate");
            mpDivideSubNode_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
            mpDivideSubNode_["bufDivideIndirectArg"] = mpIndirectArgBuffer_;
            mpDivideSubNode_["bufSvoNode"] = mpSVONodeBufferNext_;
            mpDivideSubNode_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
            mpDivideSubNode_["bufLevelAddress"] = mpLevelAddressBuffer_;
            i > 0 ? mpDivideSubNode_->executeIndirect(pContext, mpIndirectArgBuffer_.get()) : mpDivideSubNode_->execute(pContext, uint3(1));
        }
    }

#ifdef _DEBUG
    uint32_t* data = (uint32_t*)mpAtomicAndIndirect_->map(Buffer::MapType::Read);
    assert((data[ATOM_NODE_NEXT] + 1) * size_t(8) < mpSVONodeBufferNext_->getSize() / sizeof(uint32_t));
    mpAtomicAndIndirect_->unmap();

    data = (uint32_t*)mpLevelAddressBuffer_->map(Buffer::MapType::Read);
    for (uint32_t i = 0; i < kVoxelizationMeta.TotalLevel + 1; ++i) {
        std::stringstream s;
        s <<  "Dump Address Buff(Level " << i << "): " << std::to_string(data[i]) << std::endl;
        logInfo(s.str());
    }
    mpLevelAddressBuffer_->unmap();

#endif
}

void VoxelizationPass::do_build_brick(RenderContext* pContext) {

    PROFILE("BuildBrick");


    {
        PROFILE("Allocate");
        mpNodeIndirectArg_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
        mpNodeIndirectArg_->execute(pContext, uint3(1));

        mpAllocBrick_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpAllocBrick_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
        mpAllocBrick_["bufSvoNodeColor"] = mpSVONodeBufferColor_;
        mpAllocBrick_->executeIndirect(pContext, mpAtomicAndIndirect_.get(), NODE_NEXT_INDIRECT * 4);

#ifdef _DEBUG
        uint32_t* data = (uint32_t*)mpAtomicAndIndirect_->map(Buffer::MapType::Read);
        assert(data[ATOM_BRICK_NEXT] < pow(mBrickPoolResolution_ / 3, 3));
        mpAtomicAndIndirect_->unmap();
#endif
    }

    {
        PROFILE("WriteLeaf");
        mpWriteLeaf_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpWriteLeaf_["bufAtomicAndIndirect"] = mpAtomicAndIndirect_;
        mpWriteLeaf_["bufFragPosition"] = mpFragPositions_;
        mpWriteLeaf_["texPackedAlbedo"] = mpPackedAlbedo_;
        mpWriteLeaf_["texPackedNormal"] = mpPackedNormal_;
        mpWriteLeaf_["bufSvoNodeNext"] = mpSVONodeBufferNext_;
        mpWriteLeaf_["bufSvoNodeColor"] = mpSVONodeBufferColor_;
        mpWriteLeaf_["texBrickAlbedo"] = mpBrickTextures_[BRICKPOOL_COLOR];
        mpWriteLeaf_["texBrickNormal"] = mpBrickTextures_[BRICKPOOL_NORMAL];
        mpWriteLeaf_["texBrickRadius"] = mpBrickTextures_[BRICKPOOL_IRRADIANCE];
        mpWriteLeaf_->executeIndirect(pContext, mpAtomicAndIndirect_.get(), FRAG_NEXT_INDIRECT * 4);
    }


    {
        PROFILE("SpreadLeaf");
        uint3 threads = uint3(uint32_t(glm::pow(mSVOPerLevelNodeNum_[kVoxelizationMeta.TotalLevel - 1], 1.0f / 3.0f))) + uint3(1);
        uint3 groupSize = div_round_up(threads, uint3(COMMON_THREAD_SIZE));
        kVoxelizationMeta.CurLevel = kVoxelizationMeta.TotalLevel - 1;
        mpSpreadNodeLeaf_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpSpreadNodeLeaf_["CB"]["uDispathGroupSize"] = groupSize * uint3(COMMON_THREAD_SIZE);
        mpSpreadNodeLeaf_["bufLevelAddress"] = mpLevelAddressBuffer_;
        mpSpreadNodeLeaf_["bufSvoNodeColor"] = mpSVONodeBufferColor_;
        mpSpreadNodeLeaf_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_COLOR];
        mpSpreadNodeLeaf_->execute(pContext, threads);

        mpSpreadNodeLeaf_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_NORMAL];
        mpSpreadNodeLeaf_->execute(pContext, threads);
    }

    {
        PROFILE("BorderTransfer");
        uint3 threads = uint3(uint32_t(glm::pow(mSVOPerLevelNodeNum_[kVoxelizationMeta.TotalLevel - 1], 1.0f / 3.0f))) + uint3(1);
        uint3 groupSize = div_round_up(threads, uint3(COMMON_THREAD_SIZE));
        kVoxelizationMeta.CurLevel = kVoxelizationMeta.TotalLevel - 1;
        mpBorderTransfer_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpBorderTransfer_["CB"]["uDispathGroupSize"] = groupSize * uint3(COMMON_THREAD_SIZE);
        mpBorderTransfer_["CB"]["iBorderTransferAxis"] = AXIS_X;
        mpBorderTransfer_["bufLevelAddress"] = mpLevelAddressBuffer_;
        mpBorderTransfer_["bufSvoNodeColor"] = mpSVONodeBufferColor_;
        mpBorderTransfer_["bufNeighbourAddress"] = mpSVONeighbourBuffer_[AXIS_X];
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_COLOR];
        mpBorderTransfer_->execute(pContext, threads);
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_NORMAL];
        mpBorderTransfer_->execute(pContext, threads);

        mpBorderTransfer_["CB"]["iBorderTransferAxis"] = AXIS_Y;
        mpBorderTransfer_["bufNeighbourAddress"] = mpSVONeighbourBuffer_[AXIS_Y];
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_COLOR];
        mpBorderTransfer_->execute(pContext, threads);
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_NORMAL];
        mpBorderTransfer_->execute(pContext, threads);

        mpBorderTransfer_["CB"]["iBorderTransferAxis"] = AXIS_Z;
        mpBorderTransfer_["bufNeighbourAddress"] = mpSVONeighbourBuffer_[AXIS_Z];
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_COLOR];
        mpBorderTransfer_->execute(pContext, threads);
        mpBorderTransfer_["texBrickValue"] = mpBrickTextures_[BRICKPOOL_NORMAL];
        mpBorderTransfer_->execute(pContext, threads);
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
    do_rebuild_brick_buffers();
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

    mpFragPositions_ = Buffer::create(3U * size_t(mVoxelGridResolution_) * mVoxelGridResolution_ * mVoxelGridResolution_ * sizeof(uint32_t));

    kVoxelizationMeta.CellDim = cellDim;
    kVoxelizationMeta.Min = bound.minPoint;
    kVoxelizationMeta.Max = bound.maxPoint;
    kVoxelizationMeta.CellNum = kVoxelizationMeta.CellDim.x * kVoxelizationMeta.CellDim.y * kVoxelizationMeta.CellDim.z;
    kVoxelizationMeta.TotalLevel = (uint32_t)std::ceil(std::log2f((float)mVoxelGridResolution_)) + 1;
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
        mpNodeIndirectArg_ = ComputePass::create(kIndirectArgProg, "make_indirect_frag_node_arg", programDefines);
    }

    {
        mpTagNode_ = ComputePass::create(kBuildSVOProg, "tag_node", programDefines);
        Program::Desc d_divideArg;
        d_divideArg.addShaderLibrary(kBuildSVOProg).csEntry("caculate_divide_indirect_arg");
        auto pProg = ComputeProgram::create(d_divideArg, programDefines);
        mpCaculateIndirectArg_ = ComputeState::create();
        mpCaculateIndirectArg_->setProgram(pProg);
        mpCaculateIndirectArgVars_ = ComputeVars::create(pProg.get());
        mpDivideSubNode_ = ComputePass::create(kBuildSVOProg, "sub_divide_node", programDefines);
        mpBuildNeighbours_ = ComputePass::create(kBuildSVOProg, "create_neighbours", programDefines);
    }

    {
        mpAllocBrick_ = ComputePass::create(kBuildBrickProg, "alloc_brick", programDefines);
        mpWriteLeaf_ = ComputePass::create(kBuildBrickProg, "write_leaf_brick", programDefines);
        mpSpreadNodeLeaf_ = ComputePass::create(kBuildBrickProg, "spread_leaf_node", programDefines);
        mpBorderTransfer_ = ComputePass::create(kBuildBrickProg, "border_transfer", programDefines);
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

    mSVONodeNum_ = 8;
    mSVOPerLevelNodeNum_.push_back(1);
    for (uint32_t i = 1; i < kVoxelizationMeta.TotalLevel; ++i) {
        uint32_t levelNum = (uint32_t)std::powf(8, (float)i);
        mSVONodeNum_ += levelNum;
        mSVOPerLevelNodeNum_.push_back(levelNum);
    }
    mpSVONodeBufferNext_ = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));
    mpSVONodeBufferColor_ = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));
    mpSVONeighbourBuffer_[AXIS_X] = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));
    mpSVONeighbourBuffer_[AXIS_Y] = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));
    mpSVONeighbourBuffer_[AXIS_Z] = Buffer::create(mSVONodeNum_ * sizeof(uint32_t));

    mpIndirectArgBuffer_ = Buffer::create(7 * sizeof(uint32_t));
    mpLevelAddressBuffer_ = Buffer::create((kVoxelizationMeta.TotalLevel + 1) * sizeof(uint32_t));
}

void VoxelizationPass::do_rebuild_brick_buffers() {
    mpBrickTextures_[BRICKPOOL_COLOR] = Texture::create3D(mBrickPoolResolution_, mBrickPoolResolution_, mBrickPoolResolution_, ResourceFormat::RGBA8Unorm, 1,
        nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBrickTextures_[BRICKPOOL_NORMAL] = Texture::create3D(mBrickPoolResolution_, mBrickPoolResolution_, mBrickPoolResolution_, ResourceFormat::RGBA8Unorm, 1,
        nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    mpBrickTextures_[BRICKPOOL_IRRADIANCE] = Texture::create3D(mBrickPoolResolution_, mBrickPoolResolution_, mBrickPoolResolution_, ResourceFormat::RGBA8Unorm, 1,
        nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);

    kVoxelizationMeta.BrickPoolResolution = mBrickPoolResolution_;
}

void VoxelizationPass::do_clear(RenderContext* pContext) {
    PROFILE("clear");
    mpClearTexture3D_->getVars()["texClear"] = mpPackedAlbedo_;
    mpClearTexture3D_->getVars()["CB"]["uDim3"] = uint3(mVoxelGridResolution_);
    mpClearTexture3D_->execute(pContext, uint3(mVoxelGridResolution_));
    mpClearTexture3D_->getVars()["texClear"] = mpPackedNormal_;
    mpClearTexture3D_->execute(pContext, uint3(mVoxelGridResolution_));

    uint3 threads = uint3(uint32_t(glm::pow(mSVONodeNum_, 1.0f/ 3.0f))) + uint3(1);
    uint3 groups = div_round_up(threads, uint3(COMMON_THREAD_SIZE));
    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONodeBufferNext_;
    mpClearBuffer1D_->getVars()["CB"]["uDim3"] = groups;
    mpClearBuffer1D_->getVars()["CB"]["uDim1"] = mSVONodeNum_;
    mpClearBuffer1D_->execute(pContext, threads);

    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONodeBufferColor_;
    mpClearBuffer1D_->execute(pContext, threads);

    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONeighbourBuffer_[AXIS_X];
    mpClearBuffer1D_->execute(pContext, threads);

    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONeighbourBuffer_[AXIS_Y];
    mpClearBuffer1D_->execute(pContext, threads);

    mpClearBuffer1D_->getVars()["bufClear"] = mpSVONeighbourBuffer_[AXIS_Z];
    mpClearBuffer1D_->execute(pContext, threads);

    threads = uint3(uint32_t(glm::pow(3 * mVoxelGridResolution_ * mVoxelGridResolution_ * mVoxelGridResolution_, 1.0f / 3.0f))) + uint3(1);
    groups = div_round_up(threads, uint3(COMMON_THREAD_SIZE));
    mpClearBuffer1D_->getVars()["bufClear"] = mpFragPositions_;
    mpClearBuffer1D_->getVars()["CB"]["uDim3"] = groups;
    mpClearBuffer1D_->getVars()["CB"]["uDim1"] = 3 * mVoxelGridResolution_ * mVoxelGridResolution_ * mVoxelGridResolution_;
    mpClearBuffer1D_->execute(pContext, threads);

    mpClearTexture3D_["texClear"] = mpBrickTextures_[BRICKPOOL_COLOR];
    mpClearTexture3D_->getVars()["CB"]["uDim3"] = uint3(mBrickPoolResolution_);
    mpClearTexture3D_->execute(pContext, uint3(mBrickPoolResolution_));

    mpClearTexture3D_["texClear"] = mpBrickTextures_[BRICKPOOL_NORMAL];
    mpClearTexture3D_->execute(pContext, uint3(mBrickPoolResolution_));

    mpClearTexture3D_["texClear"] = mpBrickTextures_[BRICKPOOL_IRRADIANCE];
    mpClearTexture3D_->execute(pContext, uint3(mBrickPoolResolution_));

    uint32_t svoIndirectInit[7] = { 1, 1, 1, 1, 1, 1, 0 };
    mpIndirectArgBuffer_->setBlob(svoIndirectInit, 0, sizeof(uint32_t) * 7);

    std::vector<uint32_t> atomicInit;
    atomicInit.resize(BUFFER_COUNT, 0);
    atomicInit[ATOM_NODE_NEXT] = 1;
    mpAtomicAndIndirect_->setBlob(atomicInit.data(), 0, sizeof(uint32_t) * BUFFER_COUNT);

    std::vector<uint32_t> levleAddressBufInit;
    levleAddressBufInit.resize(kVoxelizationMeta.TotalLevel + 1, std::numeric_limits<uint32_t>::max());
    levleAddressBufInit[0] = 0;
    mpLevelAddressBuffer_->setBlob(levleAddressBufInit.data(), 0, sizeof(uint32_t) * (kVoxelizationMeta.TotalLevel + 1));
}
