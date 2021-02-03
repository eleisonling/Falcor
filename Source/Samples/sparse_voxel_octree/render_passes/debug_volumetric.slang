#pragma once
import Scene.Shading;
import Scene.Raster;

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"

cbuffer CB {
    voxel_meta gVoxelMeta;
}
RWByteAddressBuffer gVoxelColor;

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
        uint32_t address = linearIndex * 4;
        // load and uncompress
        uint32_t compressedColor = gVoxelColor.Load(address);
        finalColor = decode_translucent_RGBA_uint(compressedColor);
    }

    return finalColor;
}
