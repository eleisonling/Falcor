#pragma once

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
cbuffer CB {
    voxel_meta gVoxelMeta;
}
RWByteAddressBuffer gPixelPacked;

[numthreads(g_avgThreads, 1, 1)]
void cs_avg(uint3 g_threadId : SV_DispatchThreadID){

    if(g_threadId.x >= gVoxelMeta.CellDim.x * gVoxelMeta.CellDim.y * gVoxelMeta.CellDim.z) return;

    // jump to first address
    uint32_t packedAddress = g_threadId.x * 20;

    // load and uncompress
    uint4 packedColor = gPixelPacked.Load4(packedAddress);
    uint32_t pixelCount = gPixelPacked.Load(packedAddress + 16);

    if(pixelCount != 0) {
        float4 finalColor = { asfloat(packedColor.x), asfloat(packedColor.y), asfloat(packedColor.z), asfloat(packedColor.w) };
        finalColor /= pixelCount;
        gPixelPacked.Store4(packedAddress, { asuint(finalColor.x), asuint(finalColor.y), asuint(finalColor.z), asuint(finalColor.w) });
        gPixelPacked.Store(packedAddress + 16, 1);
    }
}
