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
    uint3 dimSize = uDim3 * uint3(COMMON_THREAD_SIZE);
    uint32_t index = DTid.z *  dimSize.x * dimSize.y + DTid.y * dimSize.x + DTid.x;
    if(index >= uDim1) return;
    bufClear.Store(index * 4, 0);
}

