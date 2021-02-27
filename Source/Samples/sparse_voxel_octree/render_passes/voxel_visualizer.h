#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "../shaders/voxelization_meta.slangh"


class voxel_visualizer : public std::enable_shared_from_this<voxel_visualizer> {
private:
    voxel_visualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines);
    void create_visualize_shaders(const Program::DefineList& programDefines);
    void create_visualize_resources();

    Scene::SharedPtr mpScene_ = nullptr;
    bool mDebugSVOTracing_ = true;
    voxelization_meta mVoxelizationMeta_ = {};

    Texture::SharedPtr mpVisualTexture_ = nullptr;
    FullScreenPass::SharedPtr mpVisualTracing_ = nullptr;
    Buffer::SharedPtr mpSVONodeBuffer_ = nullptr;
    
    // volumetric debug
    GraphicsVars::SharedPtr mpVisualRasterVars_ = nullptr;
    GraphicsState::SharedPtr mpVisualRaster_ = nullptr;
    TriangleMesh::SharedPtr mpRasterMesh_ = nullptr;
    Vao::SharedPtr mpRasterVao_ = nullptr;

public:
    using SharedPtr = std::shared_ptr<voxel_visualizer>;
    virtual ~voxel_visualizer();

    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void on_gui(Gui::Group& group);
    void on_render(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& pTexSampler);

    void set_voxelization_meta(const voxelization_meta& meta) { mVoxelizationMeta_ = meta; }
    void set_voxel_texture(Texture::SharedPtr pTex) { mpVisualTexture_ = pTex; }
    void set_svo_node_buffer(Buffer::SharedPtr pBuffer) { mpSVONodeBuffer_ = pBuffer; }
};
