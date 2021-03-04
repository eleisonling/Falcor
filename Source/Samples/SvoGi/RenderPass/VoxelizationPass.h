#pragma once
#include "Falcor.h"
using namespace Falcor;

#include "../Shaders/VoxelizationMeta.slangh"


class VoxelizationPass : public BaseGraphicsPass, public std::enable_shared_from_this<VoxelizationPass> {
public:

    enum EBrickPoolAttributes {
        BRICKPOOL_COLOR = 0,
        BRICKPOOL_COLOR_X,
        BRICKPOOL_COLOR_X_NEG,
        BRICKPOOL_COLOR_Y,
        BRICKPOOL_COLOR_Y_NEG,
        BRICKPOOL_COLOR_Z,
        BRICKPOOL_COLOR_Z_NEG,
        BRICKPOOL_IRRADIANCE,
        BRICKPOOL_NORMAL,

        BRICKPOOL_ATTRIBUTES_ALL,
        BRICKPOOL_ATTRIBUTES_NUM = BRICKPOOL_ATTRIBUTES_ALL
    };

    using SharedPtr = std::shared_ptr<VoxelizationPass>;
    virtual ~VoxelizationPass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    void on_gui(Gui::Group& group);
    bool need_refresh() const { return mNeedRefresh_; }

    const VoxelizationMeta& get_voxelization_meta() const;
    Buffer::SharedPtr get_svo_node_next_buffer() const { return mpSVONodeBufferNext_; }
    Buffer::SharedPtr get_svo_node_color_buffer() const { return mpSVONodeBufferColor_; }
    Texture::SharedPtr get_albedo_voxel_texture() const { return mpPackedAlbedo_; }
    Texture::SharedPtr get_normal_voxel_texture() const { return mpPackedNormal_; }
    Texture::SharedPtr get_albedo_brick_texture() const { return mpBrickTextures_[BRICKPOOL_COLOR]; }

private:
    VoxelizationPass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc, Program::DefineList& programDefines);

    void do_create_shaders(Program::DefineList& programDefines);
    void do_create_vps();
    void do_rebuild_pixel_data_buffers();
    void do_rebuild_svo_buffers();
    void do_rebuild_brick_buffers();
    void do_clear(RenderContext* pContext);
    void do_build_svo(RenderContext* pContext);
    void do_build_brick(RenderContext* pContext);

    // clear
    ComputePass::SharedPtr mpClearTexture3D_ = nullptr;
    ComputePass::SharedPtr mpClearBuffer1D_ = nullptr;

    // atomics
    Buffer::SharedPtr mpAtomicAndIndirect_ = nullptr;

    // pixel volumetric
    Scene::SharedPtr mpScene_ = nullptr;
    Buffer::SharedPtr mpViewProjections_ = nullptr;
    Texture::SharedPtr mpPackedAlbedo_ = nullptr;
    Texture::SharedPtr mpPackedNormal_ = nullptr;
    Buffer::SharedPtr mpFragPositions_ = nullptr;
    ComputePass::SharedPtr mpFragPosIndirectArg_ = nullptr;

    // pixel volumetric vars
    bool mNeedRefresh_ = true;
    const uint32_t mVoxelGridResolution_ = 256;

    // sparse Oct-tree builder
    Buffer::SharedPtr mpSVONodeBufferNext_ = nullptr;
    Buffer::SharedPtr mpSVONodeBufferColor_ = nullptr;
    Buffer::SharedPtr mpIndirectArgBuffer_ = nullptr;
    Buffer::SharedPtr mpLevelAddressBuffer_ = nullptr;
    uint32_t mSVONodeNum_ = 0;
    std::vector<uint32_t> mSVOPerLevelNodeNum_;

    ComputePass::SharedPtr mpTagNode_ = nullptr;
    ComputeState::SharedPtr mpCaculateIndirectArg_ = nullptr;
    ComputeVars::SharedPtr mpCaculateIndirectArgVars_ = nullptr;
    ComputePass::SharedPtr mpDivideSubNode_ = nullptr;
    ComputePass::SharedPtr mpNodeIndirectArg_ = nullptr;

    // brick
    const uint32_t mBrickPoolResolution_ = 70 * 3;
    const uint32_t mBrickPoolNodeResolution_ = mBrickPoolResolution_ / 2;
    Texture::SharedPtr mpBrickTextures_[BRICKPOOL_ATTRIBUTES_ALL] = {};
    ComputePass::SharedPtr mpAllocBrick_ = nullptr;
    ComputePass::SharedPtr mpWriteLeaf_ = nullptr;
    ComputePass::SharedPtr mpSpreadNodeLeaf_ = nullptr;
};
