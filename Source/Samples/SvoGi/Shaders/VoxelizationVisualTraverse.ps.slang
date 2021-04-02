#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"
import Scene.Raster;


cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
}

RWByteAddressBuffer bufSvoNodeNext;
SamplerState spTexture;
RWByteAddressBuffer bufSvoNodeColor;
Texture3D<float4> texBrickTexValue;


float4 ps_main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {

    float4 finalColor = {};

    float3 posW = vOut.posW;
    float3 worldExtent = bufVoxelMeta.Max - bufVoxelMeta.Min;
    float maxAxis = max(worldExtent.x, max(worldExtent.y, worldExtent.z));
    float3 posTex = (posW - bufVoxelMeta.Min) / float3(maxAxis);
    uint3 posCell = uint3(floor(posTex * bufVoxelMeta.CellDim));

    uint32_t foundLevel = 0;
    uint3 childRelative = uint3(0);
    float3 pMin = {};
    float3 pMax = {};
    float3 cMin = {};
    float3 cMax = {};
    float cScale = 1.0f;
    uint32_t pNodeAddress = 0;
    const float voxelStep = 1.0f / float(bufVoxelMeta.BrickPoolResolution);


    uint32_t nodeAddress = traverse_level(posCell, bufVoxelMeta.CellDim, bufVoxelMeta.CurLevel, bufSvoNodeNext, foundLevel, childRelative, pNodeAddress, pMin, pMax, cMin, cMax, cScale);
    uint32_t nodeData = bufSvoNodeNext.Load(nodeAddress * 4);
    if((foundLevel != bufVoxelMeta.TotalLevel - 1 ) || !is_valid_node(nodeData)) return finalColor;

    uint32_t leafPointer = bufSvoNodeColor.Load(get_child_index(nodeData) * 8 * 4);
    uint3 brickCoords = uint_XYZ10_to_uint3(leafPointer);
    uint32_t offset = childRelative.x | childRelative.y << 1 | childRelative.z << 2;

    // load and uncompress
#ifdef USE_SAMPLER 
    float3 cBrickUVW = (float3(brickCoords) + 0.5) / bufVoxelMeta.BrickPoolResolution;
    float3 cEnter = (posTex - cMin) / cScale;
    float3 cEnterUVW = cBrickUVW + cEnter * (2 * voxelStep);
    float4 packedColor = texBrickTexValue.SampleLevel(spTexture, cEnterUVW, 0);
#else
    float4 packedColor = texBrickTexValue[brickCoords + 2 * childOffsets[offset]];
#endif
    if (packedColor.a == 0) return finalColor; // every level may have empty nodes
    finalColor = float4(packedColor.xyz, 1.0f);

    return finalColor;
}
