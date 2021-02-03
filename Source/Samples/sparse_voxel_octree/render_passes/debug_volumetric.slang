#pragma once
import Scene.Shading;
import Scene.Raster;

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"

cbuffer CB {
    voxel_meta gVoxelMeta;
}

RWByteAddressBuffer gPixelColorSum;
RWByteAddressBuffer gPixelCount;

float4 ps_main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {
    float3 posW = vOut.posW;
    float4 finalColor = { 0 ,0, 0, 0 };
    uint3 cellIndex = floor((posW - gVoxelMeta.Min) / gVoxelMeta.CellSize);
    if(cellIndex.x >= 0 && cellIndex.x < gVoxelMeta.CellDim.x
        && cellIndex.y >= 0 && cellIndex.y < gVoxelMeta.CellDim.y
        && cellIndex.z >= 0 && cellIndex.z < gVoxelMeta.CellDim.z) {

        // get linear index
        uint32_t linearIndex = cellIndex.z * gVoxelMeta.CellDim.y * gVoxelMeta.CellDim.x + cellIndex.y * gVoxelMeta.CellDim.x + cellIndex.x;
        // jump to first address
        uint32_t colorAddress = linearIndex * 16;
        uint32_t countAddress = linearIndex * 4;

        // load and uncompress
        uint4 color = gPixelColorSum.Load4(colorAddress);
        uint count = gPixelCount.Load(countAddress);

        finalColor = { asfloat(color.x), asfloat(color.y), asfloat(color.z), asfloat(color.w) };
        finalColor /= count;
    }

    return finalColor;
}
