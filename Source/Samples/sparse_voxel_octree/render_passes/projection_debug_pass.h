#pragma once
#include "Falcor.h"

using namespace Falcor;

class debug_projection_pass : public BaseGraphicsPass, public std::enable_shared_from_this<debug_projection_pass> {
public:
    using SharedPtr = std::shared_ptr<debug_projection_pass>;
    virtual ~debug_projection_pass() override;
    static SharedPtr create(const Scene::SharedPtr& pScene, const Program::DefineList& programDefines = Program::DefineList());
    void debug_scene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);
    const int32_t get_orth_dim() const { return orthDim_; }
    void set_orth_dim(int32_t dim) { orthDim_ = dim;  }

private:
    debug_projection_pass(const Scene::SharedPtr& pScene, const Program::Desc& debugProgDesc, Program::DefineList& programDefines);
    Scene::SharedPtr mpScene_ = nullptr;
    int32_t orthDim_ = 0;
};
