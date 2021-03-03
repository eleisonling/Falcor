#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"


cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
}


RWByteAddressBuffer bufAtomicAndIndirect;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;
RWByteAddressBuffer bufFragPosition;

Texture3D<float4> texPackedAlbedo;
Texture3D<float4> texPackedNormal;

RWTexture3D<float4> texBrickAlbedo;
RWTexture3D<float4> texBrickNormal;
RWTexture3D<float4> texBrickRadius;

void alloc_3x3x3_texture_brick(uint32_t address) {
    uint32_t nextFreeTexBrick = 0;
    bufAtomicAndIndirect.InterlockedAdd(ATOM_BRICK_NEXT * 4, 1, nextFreeTexBrick);
    uint3 texAddress = uint3(0);

    uint32_t brickPoolResBricks = bufVoxelMeta.BrickPoolResolution / 3;
    texAddress = extract_index_form_linear(nextFreeTexBrick, uint3(brickPoolResBricks));
    texAddress *= uint3(3);
    bufSvoNodeColor.Store(address * 4, uint3_to_uint_XYZ10(texAddress));
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void alloc_brick(uint3 DTid : SV_DispatchThreadID){

    uint32_t nodeNextNum = bufAtomicAndIndirect.Load(ATOM_NODE_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(NODE_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= nodeNextNum) return;
   
    uint32_t tileAddress = linearIndex * 8U; 
    for(uint32_t i = 0; i < 8; ++i) {
        uint32_t address = tileAddress + i;
        alloc_3x3x3_texture_brick(address);
    }
}

void store_in_leaf(uint3 relative, uint32_t address, float4 voxelColor, float4 voxelNormal) {
    uint32_t nodeColorU = bufSvoNodeColor.Load(address * 4);
    uint3 brickCoords = uint_XYZ10_to_uint3(nodeColorU);
    uint32_t off = relative.x | relative.y << 1 | relative.z << 2;

    texBrickAlbedo[brickCoords + 2 * childOffsets[off]] = voxelColor;
    texBrickNormal[brickCoords + 2 * childOffsets[off]] = voxelNormal;
    texBrickRadius[brickCoords + 2 * childOffsets[off]] = float4(0,0,0,1);
}


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void write_leaf_brick(uint3 DTid : SV_DispatchThreadID){
    uint32_t fragPosNum = bufAtomicAndIndirect.Load(ATOM_FRAG_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(FRAG_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= fragPosNum) return;

    uint32_t cellIndexU =  bufFragPosition.Load(linearIndex * 4);
    uint3 cellIndex = uint_XYZ10_to_uint3(cellIndexU);

    float4 voxelColor = texPackedAlbedo[cellIndex];
    float4 voxelNormal = texPackedNormal[cellIndex];

    uint32_t onLevel = 0;
    uint3 relative = uint3(0);
    uint32_t nodeAddress = traverse_octree_relative_out(cellIndex, bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNodeNext, onLevel, relative);
    store_in_leaf(relative, nodeAddress, voxelColor, voxelNormal);
}

