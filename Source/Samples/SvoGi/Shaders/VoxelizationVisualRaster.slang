#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"
import Scene.Raster;

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
}

Texture3D<float4> texPackedAlbedo;
RWByteAddressBuffer bufSvoNodeNext;

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
    uint3 cellIndex = extract_index_form_linear(vIn.instanceID, bufVoxelMeta.CellDim);
    float3 worldExtent = bufVoxelMeta.Max - bufVoxelMeta.Min;
    float maxAxis = max(worldExtent.x, max(worldExtent.y, worldExtent.z));
    float3 cellSize = float3(maxAxis) / float3(bufVoxelMeta.CellDim);

    vIn.pos *= cellSize;
    float3 instancePos = vIn.pos + bufVoxelMeta.Min + float3(cellIndex.x + 0.5f, cellIndex.y + 0.5f, cellIndex.z + 0.5f) * cellSize;
    vOut.posW = instancePos;
    vOut.posH = mul(float4(instancePos, 1), gScene.camera.getViewProj());

    return vOut;
}

float4 ps_main(vs_out vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {

    float4 finalColor = { 0 ,0, 0, 0 };
    uint32_t pixelCount = 0;
    float3 posW = vOut.posW;
    float3 worldExtent = bufVoxelMeta.Max - bufVoxelMeta.Min;
    float maxAxis = max(worldExtent.x, max(worldExtent.y, worldExtent.z));
    float3 posTex = (posW - bufVoxelMeta.Min) / float3(maxAxis);
    uint3 cellIndex = uint3(floor(posTex * (bufVoxelMeta.CellDim - 1)));

    uint32_t foundLevel = 0;
    uint32_t nodeNext = traverse_octree_simple(cellIndex, bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNodeNext, foundLevel);
    uint32_t leafPointer = get_child_index(bufSvoNodeNext.Load(nodeNext * 4));

    if(foundLevel != bufVoxelMeta.TotalLevel || leafPointer == 0) discard;
    cellIndex = extract_index_form_linear(leafPointer, bufVoxelMeta.CellDim);

    // load and uncompress
    float4 packedColor = texPackedAlbedo[cellIndex];
    finalColor = float4(packedColor.xyz, 1.0f);

    return finalColor;
}
