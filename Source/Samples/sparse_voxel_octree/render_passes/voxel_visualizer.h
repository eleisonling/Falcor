#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "voxel_meta.slangh"


class voxel_visualizer : public std::enable_shared_from_this<voxel_visualizer> {
private:
    voxel_visualizer(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines);
    void create_visualize_shaders(const Program::DefineList& programDefines);
    void create_visualize_resources();

    Scene::SharedPtr mpScene_ = nullptr;
    bool debugSVOTracing_ = true;
    float mipLevel_ = 0.f;
    svo_meta svoMeta_ = {};
    voxel_meta voxelMeta_ = {};

    Texture::SharedPtr mpVisualTexture_ = nullptr;
    // debug tracing svo
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
    void on_gui_render(Gui::Group& group);
    void on_execute(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo, const Sampler::SharedPtr& pTexSampler);

    void set_svo_meta(const svo_meta& meta) { svoMeta_ = meta; }
    void set_voxel_meta(const voxel_meta& meta) { voxelMeta_ = meta; }
    void set_visual_texture(Texture::SharedPtr tex) { mpVisualTexture_ = tex; }
    void set_svo_node_buffer(Buffer::SharedPtr buffer) { mpSVONodeBuffer_ = buffer; }
};
