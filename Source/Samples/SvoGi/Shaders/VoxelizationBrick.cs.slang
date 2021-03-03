#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
}


RWByteAddressBuffer bufAtomicAndIndirect;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;

Texture3D<float4> texPackedAlbedo;
Texture3D<float4> texPackedNormal;

Texture3D<float4> texBrickAlbedo;
Texture3D<float4> texBrickNormal;
Texture3D<float4> texBrickRadius;

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

void store_in_leaf(uint3 localCellIndex, uint32_t address, float4 voxelColor, float4 voxelNormal) {
}


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void write_leaf_brick(uint3 DTid : SV_DispatchThreadID){
    uint32_t fragPosNum = bufAtomicAndIndirect.Load(ATOM_FRAG_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(FRAG_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= fragPosNum) return;

    uint32_t cellIndexU =  bufFragPosition.Load(linearIndex * 4);
    uint3 cellIndex = uint_XYZ10_to_uint3(cellIndexU);

    float4 voxelColor = texPackedAlbedo[cellIndex]
    float4 voxelNormal = texPackedNormal[cellIndex]

    uint32_t onLevel = 0;
    uint32_t nodeAddress = traverse_octree_pos_out(cellIndex, bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNodeNext, onLevel);

    store_in_leaf(cellIndex, nodeAddress, voxelColor, voxelNormal);
}

