#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "../Shaders/VoxelizationMeta.slangh"


class VoxelVisualizer : public std::enable_shared_from_this<VoxelVisualizer> {
private:

    VoxelVisualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines);
    void create_visualize_shaders(const Program::DefineList& programDefines);
    void create_visualize_resources();
    void do_visual_voxel(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    void do_visual_brick(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);

    Scene::SharedPtr mpScene_ = nullptr;
    bool mUserTacing_ = false;
    uint32_t mType_ = 0;
    VoxelizationMeta mVoxelizationMeta_ = {};

    Texture::SharedPtr mpVoxelTexture_ = nullptr;
    Texture::SharedPtr mpBrickAlbedoTexture_ = nullptr;
    Sampler::SharedPtr mpSampler_ = nullptr;
    FullScreenPass::SharedPtr mpVisualTracing_ = nullptr;
    Buffer::SharedPtr mpSVONodeNextBuffer_ = nullptr;
    Buffer::SharedPtr mpSVONodeColorBuffer_ = nullptr;
    
    GraphicsVars::SharedPtr mpVisualVarsR_[2] = {};
    GraphicsState::SharedPtr mpVisualR_[2] = {};

    TriangleMesh::SharedPtr mpRasterMesh_ = nullptr;
    Vao::SharedPtr mpRasterVao_ = nullptr;

public:
    using SharedPtr = std::shared_ptr<VoxelVisualizer>;
    virtual ~VoxelVisualizer();

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui(Gui::Group& group);
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);

    void set_voxelization_meta(const VoxelizationMeta& meta) { mVoxelizationMeta_ = meta; }
    void set_voxel_texture(Texture::SharedPtr pTex) { mpVoxelTexture_ = pTex; }
    void set_brick_albedo_texture(Texture::SharedPtr pTex) { mpBrickAlbedoTexture_ = pTex; }
    void set_svo_node_next_buffer(Buffer::SharedPtr pBuffer) { mpSVONodeNextBuffer_ = pBuffer; }
    void set_svo_node_color_buffer(Buffer::SharedPtr pBuffer) { mpSVONodeColorBuffer_ = pBuffer; }
    void set_texture_sampler(Sampler::SharedPtr pSampler) { mpSampler_ = nullptr; }
};
