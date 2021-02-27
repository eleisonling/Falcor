#pragma once

#include "Samples/SvoGi/Shaders/LightInjectionMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"

import Scene.Shading;
import Scene.Raster;

cbuffer CB {
    VoxelizationMeta bufVoxelizationMeta;
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
