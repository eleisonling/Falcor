#include "VoxlizationPass.h"

namespace {
    static std::string kVolumetricProg = "Samples/SvoGi/shaders/voxelization.slang";
    static std::string kBuildSVOProg = "Samples/SvoGi/shaders/voxelization_svo.cs.slang";
    static voxelization_meta kVoxelizationMeta{};
}

VoxlizationPass::~VoxlizationPass() {
    mpScene_ = nullptr;
    mpViewProjections_ = nullptr;
    mpPackedAlbedo_ = nullptr;
    mpPackedNormal_ = nullptr;

    mpSVONodeBuffer_ = nullptr;
    mpIndirectArgBuffer_ = nullptr;
    mpTagNode_ = nullptr;
    mpTagNodeVars_ = nullptr;
    mpCaculateIndirectArg_ = nullptr;
    mpCaculateIndirectArgVars_ = nullptr;
    mpDivideSubNode_ = nullptr;
    mpDivideSubNodeVars_ = nullptr;
}

VoxlizationPass::SharedPtr VoxlizationPass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_volumetric;
    d_volumetric.addShaderLibrary(kVolumetricProg)
        .vsEntry("")
        .gsEntry("gs_main")
        .psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new VoxlizationPass(pScene, d_volumetric, dl));
}

void VoxlizationPass::on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("voxelization");

    // do clear
    pContext->clearUAV(mpPackedAlbedo_->getUAV().get(), uint4(0, 0, 0 ,0));
    pContext->clearUAV(mpPackedNormal_->getUAV().get(), uint4(0, 0, 0 ,0));


    mpState->setFbo(pDstFbo);
    mpVars["texPackedAlbedo"] = mpPackedAlbedo_;
    mpVars["texPackedNormal"] = mpPackedNormal_;
    mpVars["CB"]["matViewProjections"] = mpViewProjections_;
    mpVars["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    do_build_svo(pContext);
    mNeedRefresh_ = false;
}

void VoxlizationPass::do_build_svo(RenderContext* pContext) {

    PROFILE("build svo");
    pContext->clearUAV(mpSVONodeBuffer_->getUAV().get(), uint4{ 0, 0, 0, 0 });
    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_->setBlob(initialValue, 0, sizeof(uint32_t) * 7);

    uint3 tagGroup = { (kVoxelizationMeta.CellDim.x + COMMON_THREAD_SIZE - 1) / COMMON_THREAD_SIZE, (kVoxelizationMeta.CellDim.y + COMMON_THREAD_SIZE - 1) / COMMON_THREAD_SIZE,
        (kVoxelizationMeta.CellDim.z + COMMON_THREAD_SIZE - 1) / COMMON_THREAD_SIZE };

    for (uint32_t i = 1; i <= kVoxelizationMeta.TotalLevel; ++i) {
        // tag
        kVoxelizationMeta.CurLevel = i;
        // bound resource to shader
        mpTagNodeVars_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        mpTagNodeVars_["texPackedAlbedo"] = mpPackedAlbedo_;
        mpTagNodeVars_["bufSvoNode"] = mpSVONodeBuffer_;

        mpTagNodeVars_["CB"]["bufVoxelMeta"].setBlob(kVoxelizationMeta);
        pContext->dispatch(mpTagNode_.get(), mpTagNodeVars_.get(), tagGroup);

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

void VoxlizationPass::do_fixture_cell_size() {
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil((bound.extent() + mCellSize_) / mCellSize_);
    uint32_t maxDim = std::max(cellDim.x, std::max(cellDim.y, cellDim.z));

    uint32_t totalLevel = (uint32_t)std::ceil(std::log2f((float)maxDim));
    maxDim = (uint32_t)std::pow(2, totalLevel) - 1;

    float maxSceneDim = std::max(bound.extent().x, std::max(bound.extent().y, bound.extent().z));
    mCellSize_ = maxSceneDim / maxDim;
}

void VoxlizationPass::on_gui(Gui::Group& group) {
    mRebuildBuffer_ |= group.var("Cell Size", mCellSize_);
    if (group.button("Rebuild")) {
        if (mRebuildBuffer_) {
            do_fixture_cell_size();
            do_rebuild_pixel_data_buffers();
            do_rebuild_svo_buffers();
        }
        mNeedRefresh_ = true;
    }
}

const voxelization_meta& VoxlizationPass::get_voxelization_meta() const {
    return kVoxelizationMeta;
}

VoxlizationPass::VoxlizationPass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, Program::DefineList& programDefines)
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
    do_create_svo_shaders(programDefines);
    do_fixture_cell_size();
    do_rebuild_pixel_data_buffers();
    do_rebuild_svo_buffers();
}

float3 next_pow_2(float3 v) {
    return { std::pow(2, std::ceil(std::log2(v.x))), std::pow(2, std::ceil(std::log2(v.y))), std::pow(2, std::ceil(std::log2(v.z))) };
}

void VoxlizationPass::do_rebuild_pixel_data_buffers() {

    mRebuildBuffer_ = false;
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil((bound.extent() + mCellSize_) / mCellSize_ );
    cellDim = uint3(next_pow_2(cellDim));
    {
        size_t bufferSize = size_t(cellDim.x) * cellDim.y * cellDim.z * sizeof(uint32_t);
        if (!mpPackedAlbedo_ || mpPackedAlbedo_->getWidth() != cellDim.x ||  mpPackedAlbedo_->getHeight() != cellDim.y || mpPackedAlbedo_->getDepth() != cellDim.z) {
            mpPackedAlbedo_ = Texture::create3D(cellDim.x, cellDim.y, cellDim.z, ResourceFormat::RGBA8Unorm, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
        }

        if (!mpPackedNormal_ || mpPackedNormal_->getWidth() != cellDim.x ||  mpPackedNormal_->getHeight() != cellDim.y || mpPackedNormal_->getDepth() != cellDim.z) {
            mpPackedNormal_ = Texture::create3D(cellDim.x, cellDim.y, cellDim.z, ResourceFormat::RGBA8Unorm, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
        }
    }

    kVoxelizationMeta.CellDim = cellDim;
    kVoxelizationMeta.CellSize = mCellSize_;
    kVoxelizationMeta.Min = bound.minPoint;
    kVoxelizationMeta.Max = bound.maxPoint;
  }


void VoxlizationPass::do_create_svo_shaders(Program::DefineList& programDefines) {
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

void VoxlizationPass::do_create_vps() {
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

void VoxlizationPass::do_rebuild_svo_buffers() {
    uint32_t maxDim = std::max(std::max(kVoxelizationMeta.CellDim.x, kVoxelizationMeta.CellDim.y), kVoxelizationMeta.CellDim.z);
    kVoxelizationMeta.TotalLevel = (uint32_t)std::ceil(std::log2f((float)maxDim));
    assert(kVoxelizationMeta.TotalLevel <= MAX_LEVEL);
    maxDim = (uint32_t)std::pow(2, kVoxelizationMeta.TotalLevel);
    kVoxelizationMeta.CellDim = kVoxelizationMeta.CellDim;
    kVoxelizationMeta.CellNum = kVoxelizationMeta.CellDim.x * kVoxelizationMeta.CellDim.y * kVoxelizationMeta.CellDim.z;
    kVoxelizationMeta.SvoDim = uint3(maxDim, maxDim, maxDim);
    kVoxelizationMeta.Min = kVoxelizationMeta.Min;
    kVoxelizationMeta.CellSize = kVoxelizationMeta.CellSize;

    size_t bufferSize = 0;
    for (uint32_t i = 1; i <= kVoxelizationMeta.TotalLevel; ++i) {
        bufferSize += (size_t)std::powf(8, (float)i);
    }
    bufferSize *= sizeof(uint32_t);
    mpSVONodeBuffer_ = Buffer::create(bufferSize);

    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_ = Buffer::create(7 * sizeof(uint32_t), ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, Buffer::CpuAccess::None, initialValue);
}
