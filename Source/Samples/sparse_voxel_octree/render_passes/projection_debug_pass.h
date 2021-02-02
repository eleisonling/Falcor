#pragma once
#include "Falcor.h"

using namespace Falcor;

class projection_debug_pass : public BaseGraphicsPass, public std::enable_shared_from_this<projection_debug_pass> {
public:
    using SharedPtr = std::shared_ptr<projection_debug_pass>;
    virtual ~projection_debug_pass() override;
    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    const uint32_t get_orth_dim() const { return orthDim_; }
    void set_orth_dim(uint32_t dim) { orthDim_ = dim;  }
    void onGuiRender(Gui::Group& group);

private:
    projection_debug_pass(const Scene::SharedPtr& pScene, const Program::Desc& debugProgDesc, Program::DefineList& programDefines);
    Scene::SharedPtr mpScene_ = nullptr;
    uint32_t orthDim_ = 0;
};
