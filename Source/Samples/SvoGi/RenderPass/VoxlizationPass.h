#pragma once
#include "Falcor.h"
using namespace Falcor;

#include "../Shaders/VoxelizationMeta.slangh"


class VoxlizationPass : public BaseGraphicsPass, public std::enable_shared_from_this<VoxlizationPass> {
public:
    using SharedPtr = std::shared_ptr<VoxlizationPass>;
    virtual ~VoxlizationPass() override;

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    void on_gui(Gui::Group& group);
    bool need_refresh() const { return mNeedRefresh_; }

    const VoxelizationMeta& get_voxelization_meta() const;
    Buffer::SharedPtr get_svo_node_buffer() const { return mpSVONodeBuffer_; }
    Texture::SharedPtr get_albedo_voxel_texture() const { return mpPackedAlbedo_; }
    Texture::SharedPtr get_normal_voxel_texture() const { return mpPackedNormal_; }

private:
    VoxlizationPass(const Scene::SharedPtr& pScene, const Program::Desc& volumetricProgDesc,Program::DefineList& programDefines);

    void do_create_svo_shaders(Program::DefineList& programDefines);
    void do_create_vps();
    void do_rebuild_pixel_data_buffers();
    void do_rebuild_svo_buffers();
    void do_build_svo(RenderContext* pContext);
    void do_fixture_cell_size();

    // pixel volumetric
    Scene::SharedPtr mpScene_ = nullptr;
    Buffer::SharedPtr mpViewProjections_ = nullptr;
    Texture::SharedPtr mpPackedAlbedo_ = nullptr;
    Texture::SharedPtr mpPackedNormal_ = nullptr;

    // pixel volumetric vars
    bool mNeedRefresh_ = true;
    bool mRebuildBuffer_ = false;
    float mCellSize_ = 1.0f;

    // sparse Oct-tree builder
    Buffer::SharedPtr mpSVONodeBuffer_ = nullptr;
    Buffer::SharedPtr mpIndirectArgBuffer_ = nullptr;

    ComputeState::SharedPtr mpTagNode_ = nullptr;
    ComputeVars::SharedPtr mpTagNodeVars_ = nullptr;
    ComputeState::SharedPtr mpCaculateIndirectArg_ = nullptr;
    ComputeVars::SharedPtr mpCaculateIndirectArgVars_ = nullptr;
    ComputeState::SharedPtr mpDivideSubNode_ = nullptr;
    ComputeVars::SharedPtr mpDivideSubNodeVars_ = nullptr;
};
