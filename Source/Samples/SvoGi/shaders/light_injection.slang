#pragma once

#include "Samples/SvoGi/shaders/light_injection_meta.slangh"
#include "Samples/SvoGi/shaders/voxelization_meta.slangh"

import Scene.Shading;
import Scene.Raster;

cbuffer CB {
    voxelization_meta bufVoxelizationMeta;
    float4x4 matShadowMatrix;
}

Texture3D<float4> texAlbedo;
Texture3D<float4> texNormal;
Texture2D<float> texShadowMap;
RWTexture3D<float4> texRadius;

[numthreads(INJECTION_THREAD_SIZE, INJECTION_THREAD_SIZE, INJECTION_THREAD_SIZE)]
void light_injection(uint3 DTid : SV_DispatchThreadID){
    if(any(DTid >= bufVoxelizationMeta.CellDim)) return;
}
