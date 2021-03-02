#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"

RWTexture3D<uint32_t> texClear;
RWByteAddressBuffer bufClear;

cbuffer CB {
   uint3 uDim3;
   uint32_t uDim1;
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void clear_texture_3d(uint3 DTid : SV_DispatchThreadID){
    if(any(DTid >= uDim3)) return;
    texClear[DTid] = 0;
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void clear_buffer_linear(uint3 DTid : SV_DispatchThreadID){
    uint32_t index = DTid.z *  uDim3.x * uDim3.y + DTid.y * uDim3.x + DTid.x;
    if(index >= uDim1) return;
    bufClear.Store(index * 4, 0);
}

