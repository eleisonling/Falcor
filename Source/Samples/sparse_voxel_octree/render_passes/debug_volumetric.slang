#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
import Scene.Raster;

cbuffer CB {
    voxel_meta gVoxelMeta;
}

RWByteAddressBuffer gPixelColorSum;
RWByteAddressBuffer gPixelCount;

struct vs_in {
    float3 pos                      : POSITION;
    float3 normal                   : NORMAL;
    float2 texC                     : TEXCOORD;
    uint instanceID                 : SV_InstanceID;
    uint vertexID                   : SV_VertexID;
};

struct vs_out {
    linear float3 posW              : POSW;     
    float4 posH                     : SV_POSITION;
};

uint3 extract_index_form_linear(uint32_t linearIndex) {
    uint32_t z = linearIndex / (gVoxelMeta.CellDim.x * gVoxelMeta.CellDim.y);
    linearIndex -= (z * gVoxelMeta.CellDim.x * gVoxelMeta.CellDim.y);
    uint32_t y = linearIndex / gVoxelMeta.CellDim.x;
    uint32_t x = linearIndex % gVoxelMeta.CellDim.x;

    return { x, y, z };
}

vs_out vs_main(vs_in vIn)
{
    vs_out vOut = {};

    // from instance id to input position
    uint3 cellIndex = extract_index_form_linear(vIn.instanceID);
    float3 instancePos = vIn.pos + gVoxelMeta.Min +
        float3(cellIndex.x + 0.5f, cellIndex.y + 0.5f, cellIndex.z + 0.5f) * float3(gVoxelMeta.CellSize, gVoxelMeta.CellSize, gVoxelMeta.CellSize);
    vOut.posW = instancePos;
    vOut.posH = mul(float4(instancePos, 1), gScene.camera.getViewProj());

    return vOut;
}

float4 ps_main(vs_out vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {

    float4 finalColor = { 0 ,0, 0, 0 };
    uint32_t pixelCount = 0;
    float3 posW = vOut.posW;
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
        pixelCount = gPixelCount.Load(countAddress);

        if(pixelCount != 0) {
            finalColor = { asfloat(color.x), asfloat(color.y), asfloat(color.z), asfloat(color.w) };
            finalColor /= pixelCount;
        }
    }

    if(pixelCount == 0) discard;

    return finalColor;
}
