#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "../Shaders/VoxelizationMeta.slangh"


class VoxelVisualizer : public std::enable_shared_from_this<VoxelVisualizer> {
private:
    VoxelVisualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines);
    void create_visualize_shaders(const Program::DefineList& programDefines);
    void create_visualize_resources();

    Scene::SharedPtr mpScene_ = nullptr;
    bool mDebugSVOTracing_ = true;
    VoxelizationMeta mVoxelizationMeta_ = {};

    Texture::SharedPtr mpVisualTexture_ = nullptr;
    FullScreenPass::SharedPtr mpVisualTracing_ = nullptr;
    Buffer::SharedPtr mpSVONodeNextBuffer_ = nullptr;
    
    // volumetric debug
    GraphicsVars::SharedPtr mpVisualRasterVars_ = nullptr;
    GraphicsState::SharedPtr mpVisualRaster_ = nullptr;
    TriangleMesh::SharedPtr mpRasterMesh_ = nullptr;
    Vao::SharedPtr mpRasterVao_ = nullptr;

public:
    using SharedPtr = std::shared_ptr<VoxelVisualizer>;
    virtual ~VoxelVisualizer();

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui(Gui::Group& group);
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& pTexSampler);

    void set_voxelization_meta(const VoxelizationMeta& meta) { mVoxelizationMeta_ = meta; }
    void set_voxel_texture(Texture::SharedPtr pTex) { mpVisualTexture_ = pTex; }
    void set_svo_node_next_buffer(Buffer::SharedPtr pBuffer) { mpSVONodeNextBuffer_ = pBuffer; }
};
