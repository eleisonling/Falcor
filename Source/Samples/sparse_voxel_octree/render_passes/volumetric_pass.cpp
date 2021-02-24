#include "volumetric_pass.h"

namespace {
    static std::string kVolumetricProg = "Samples/sparse_voxel_octree/render_passes/volumetric.slang";
    static std::string kBuildSVOProg = "Samples/sparse_voxel_octree/render_passes/build_svo.cs.slang";
    static voxel_meta kVoxelMeta{};
    static svo_meta kSvoMeta{};
}

volumetric_pass::~volumetric_pass() {
    mpScene_ = nullptr;
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

volumetric_pass::SharedPtr volumetric_pass::create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines /*= Program::DefineList()*/) {

    Program::Desc d_volumetric;
    d_volumetric.addShaderLibrary(kVolumetricProg)
        .vsEntry("")
        .gsEntry("gs_main")
        .psEntry("ps_main");

    if (pScene == nullptr) throw std::exception("Can't create a RasterScenePass object without a scene");
    Program::DefineList dl = programDefines;
    dl.add(pScene->getSceneDefines());

    return SharedPtr(new volumetric_pass(pScene, d_volumetric, dl));
}

void volumetric_pass::volumetric_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo) {
    PROFILE("do volumetric");

    // do clear
    for (uint32_t mip = 0; mip < mpPackedAlbedo_->getMipCount(); mip++) {
        pContext->clearUAV(mpPackedAlbedo_->getUAV(mip).get(), uint4(0, 0, 0 ,0));
    }

    for (uint32_t mip = 0; mip < mpPackedNormal_->getMipCount(); mip++) {
        pContext->clearUAV(mpPackedNormal_->getUAV(mip).get(), uint4(0, 0, 0 ,0));
    }

    mpState->setFbo(pDstFbo);
    mpScene_->rasterize(pContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    build_svo(pContext);
    needRefresh_ = false;
}

void volumetric_pass::build_svo(RenderContext* pContext) {

    PROFILE("build svo");
    pContext->clearUAV(mpSVONodeBuffer_->getUAV().get(), uint4{ 0, 0, 0, 0 });
    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_->setBlob(initialValue, 0, sizeof(uint32_t) * 7);

    uint3 tagGroup = { (kSvoMeta.CellDim.x + g_tagThreads - 1) / g_tagThreads, (kSvoMeta.CellDim.y + g_tagThreads - 1) / g_tagThreads,
        (kSvoMeta.CellDim.z + g_tagThreads - 1) / g_tagThreads };

    for (uint32_t i = 1; i <= kSvoMeta.TotalLevel; ++i) {
        // tag
        kSvoMeta.CurLevel = i;
        mpTagNodeVars_["CB"]["gSvoMeta"].setBlob(kSvoMeta);
        pContext->dispatch(mpTagNode_.get(), mpTagNodeVars_.get(), tagGroup);

        if (i < kSvoMeta.TotalLevel) {
            // calculate indirect
            pContext->dispatch(mpCaculateIndirectArg_.get(), mpCaculateIndirectArgVars_.get(), uint3{ 1 ,1 ,1 });

            // sub-divide
            pContext->dispatchIndirect(mpDivideSubNode_.get(), mpDivideSubNodeVars_.get(), mpIndirectArgBuffer_.get(), 0);
        }
    }
}

void volumetric_pass::fixture_cell_size() {
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil((bound.extent() + cellSize_) / cellSize_);
    uint32_t maxDim = std::max(cellDim.x, std::max(cellDim.y, cellDim.z));

    uint32_t totalLevel = (uint32_t)std::ceil(std::log2f((float)maxDim));
    maxDim = (uint32_t)std::pow(2, totalLevel) - 1;

    float maxSceneDim = std::max(bound.extent().x, std::max(bound.extent().y, bound.extent().z));
    cellSize_ = maxSceneDim / maxDim;
}

void volumetric_pass::on_gui_render(Gui::Group& group) {
    rebuildBuffer_ |= group.var("Cell Size", cellSize_);
    if (group.button("Rebuild")) {
        if (rebuildBuffer_) {
            fixture_cell_size();
            rebuild_pixel_data_buffers();
            rebuild_svo_buffers();
        }
        needRefresh_ = true;
    }
}

const voxel_meta& volumetric_pass::get_voxel_meta() const {
    return kVoxelMeta;
}

const svo_meta& volumetric_pass::get_svo_meta() const {
    return kSvoMeta;
}

volumetric_pass::volumetric_pass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, Program::DefineList& programDefines)
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

    create_svo_shaders(programDefines);
    fixture_cell_size();
    rebuild_pixel_data_buffers();
    rebuild_svo_buffers();
}

float3 next_pow_2(float3 v) {
    return { std::pow(2, std::ceil(std::log2(v.x))), std::pow(2, std::ceil(std::log2(v.y))), std::pow(2, std::ceil(std::log2(v.z))) };
}

void volumetric_pass::rebuild_pixel_data_buffers() {

    rebuildBuffer_ = false;
    auto& bound = mpScene_->getSceneBounds();
    glm::uvec3 cellDim = glm::ceil((bound.extent() + cellSize_) / cellSize_ );
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

    kVoxelMeta.CellDim = cellDim;
    kVoxelMeta.CellSize = cellSize_;
    kVoxelMeta.Min = bound.minPoint;
    kVoxelMeta.Max = bound.maxPoint;

    mpVars["gPackedAlbedo"] = mpPackedAlbedo_;
    mpVars["gPackedNormal"] = mpPackedNormal_;
    mpVars["CB"]["gVoxelMeta"].setBlob(kVoxelMeta);
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

void volumetric_pass::rebuild_svo_buffers() {
    uint32_t maxDim = std::max(std::max(kVoxelMeta.CellDim.x, kVoxelMeta.CellDim.y), kVoxelMeta.CellDim.z);
    kSvoMeta.TotalLevel = (uint32_t)std::ceil(std::log2f((float)maxDim));
    assert(kSvoMeta.TotalLevel <= g_maxLevel);
    maxDim = (uint32_t)std::pow(2, kSvoMeta.TotalLevel);
    kSvoMeta.CellDim = kVoxelMeta.CellDim;
    kSvoMeta.CellNum = kSvoMeta.CellDim.x * kSvoMeta.CellDim.y * kSvoMeta.CellDim.z;
    kSvoMeta.SvoDim = uint3(maxDim, maxDim, maxDim);
    kSvoMeta.Min = kVoxelMeta.Min;
    kSvoMeta.CellSize = kVoxelMeta.CellSize;

    size_t bufferSize = 0;
    for (uint32_t i = 1; i <= kSvoMeta.TotalLevel; ++i) {
        bufferSize += (size_t)std::powf(8, (float)i);
    }
    bufferSize *= sizeof(uint32_t);
    mpSVONodeBuffer_ = Buffer::create(bufferSize);

    uint32_t initialValue[7] = { 1, 1, 1, 0, 1, 0, 0 };
    mpIndirectArgBuffer_ = Buffer::create(7 * sizeof(uint32_t), ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, Buffer::CpuAccess::None, initialValue);

    // bound resource to shader
    mpTagNodeVars_["CB"]["gSvoMeta"].setBlob(kSvoMeta);
    mpTagNodeVars_["gPackedAlbedo"] = mpPackedAlbedo_;
    mpTagNodeVars_["gSvoNodeBuffer"] = mpSVONodeBuffer_;

    mpCaculateIndirectArgVars_["gDivideIndirectArg"] = mpIndirectArgBuffer_;

    mpDivideSubNodeVars_["gDivideIndirectArg"] = mpIndirectArgBuffer_;
    mpDivideSubNodeVars_["gSvoNodeBuffer"] = mpSVONodeBuffer_;
}
