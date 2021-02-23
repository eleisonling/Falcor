#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
import Scene.Raster;

cbuffer CB {
    voxel_meta gVoxelMeta;
    uint32_t gMip;
}

Texture3D<float4> gPackedAlbedo;
SamplerState g_texSampler;

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

vs_out vs_main(vs_in vIn)
{
    vs_out vOut = {};

    // from instance id to input position
    uint3 cellIndex = extract_index_form_linear(vIn.instanceID, gVoxelMeta.CellDim);
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
    float3 texCoord = { (float)cellIndex.x / (float)gVoxelMeta.CellDim.x, (float)cellIndex.y / (float)gVoxelMeta.CellDim.y, (float)cellIndex.z / (float)gVoxelMeta.CellDim.z };


    if(cellIndex.x >= 0 && cellIndex.x < gVoxelMeta.CellDim.x
        && cellIndex.y >= 0 && cellIndex.y < gVoxelMeta.CellDim.y
        && cellIndex.z >= 0 && cellIndex.z < gVoxelMeta.CellDim.z) {

        // load and uncompress
        float4 packedColor = gPackedAlbedo.SampleLevel(g_texSampler, texCoord, gMip);
        pixelCount = packedColor.a * g_packRGBFactor;
        finalColor = float4(packedColor.xyz, 1.0f);
    }

    if(pixelCount == 0) discard;
    return finalColor;
}
