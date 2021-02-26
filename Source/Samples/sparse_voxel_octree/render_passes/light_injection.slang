#pragma once

#include "Samples/sparse_voxel_octree/render_passes/injection_meta.slangh"
#include "Samples/sparse_voxel_octree/shaders/voxelization_meta.slangh"

import Scene.Shading;
import Scene.Raster;

cbuffer CB {
    voxelization_meta g_svoMeta;
    float4x4 g_shadow_matrix;
}

Texture3D<float4> g_albedo;
Texture3D<float4> g_normal;
Texture2D<float> g_shadowmap;
RWTexture3D<float4> g_radius;

[numthreads(INJECTION_THREAD_SIZE, INJECTION_THREAD_SIZE, INJECTION_THREAD_SIZE)]
void light_injection(uint3 DTid : SV_DispatchThreadID){
    if(any(DTid >= g_svoMeta.CellDim)) return;
}
