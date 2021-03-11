#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"


cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
    uint3 uDispathGroupSize;
    int32_t iBorderTransferAxis;
}


RWByteAddressBuffer bufAtomicAndIndirect;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;
RWByteAddressBuffer bufFragPosition;
RWByteAddressBuffer bufLevelAddress;
RWByteAddressBuffer bufNeighbourAddress;

Texture3D<float4> texPackedAlbedo;
Texture3D<float4> texPackedNormal;

RWTexture3D<float4> texBrickAlbedo;
RWTexture3D<float4> texBrickNormal;
RWTexture3D<float4> texBrickRadius;

uint3 alloc_3x3x3_texture_brick() {
    uint32_t nextFreeTexBrick = 0;
    bufAtomicAndIndirect.InterlockedAdd(ATOM_BRICK_NEXT * 4, 1, nextFreeTexBrick);
    uint3 texAddress = uint3(0);
    uint32_t brickPoolResBricks = bufVoxelMeta.BrickPoolResolution / 3;
    texAddress = extract_index_form_linear(nextFreeTexBrick, uint3(brickPoolResBricks));
    texAddress *= uint3(3);
    return texAddress;
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void alloc_brick(uint3 DTid : SV_DispatchThreadID){

    uint32_t nodeNextNum = bufAtomicAndIndirect.Load(ATOM_NODE_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(NODE_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= nodeNextNum) return;
   
    uint32_t tileAddress = linearIndex * 8U; 
    uint3 brick3x3Address = alloc_3x3x3_texture_brick();

    for(uint32_t i = 0; i < 8; ++i) {
        uint32_t address = tileAddress + i;
        bufSvoNodeColor.Store(address * 4, uint3_to_uint_XYZ10(brick3x3Address));
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
    uint32_t nodeAddress = traverse_return_child_relative(cellIndex, bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNodeNext, onLevel, relative);
    store_in_leaf(relative, nodeAddress, voxelColor, voxelNormal);
}

RWTexture3D<float4> texBrickValue;

void load_brick_values(uint3 brickAddress, out float4[8] brickValues) {
    for(int32_t i = 0; i < 8; ++i) {
        brickValues[i] = texBrickValue[brickAddress + 2 * childOffsets[i]];
    }
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void spread_leaf_node(uint3 DTid : SV_DispatchThreadID){
    uint32_t linearIndex = DTid.z * uDispathGroupSize.y * uDispathGroupSize.x + DTid.y * uDispathGroupSize.x + DTid.x;
    uint32_t levelStart = bufLevelAddress.Load((bufVoxelMeta.CurLevel) * 4);
    uint32_t levelNextStart = bufVoxelMeta.CurLevel <= (bufVoxelMeta.TotalLevel) ? bufLevelAddress.Load((bufVoxelMeta.CurLevel + 1) * 4) : 0;

    uint32_t nodeAddress = levelStart + linearIndex;
    if(nodeAddress >= levelNextStart) return;

    uint32_t brickAddressU = bufSvoNodeColor.Load(nodeAddress * 4 * 8);
    uint3 brickAddress = uint_XYZ10_to_uint3(brickAddressU);

    float4 brickValue[8] = {};
    load_brick_values(brickAddress, brickValue);

    float4 col = float4(0);

    // center
    for(int32_t i = 0; i < 8; ++i) {
        col += 0.125f * brickValue[i];
    }
    texBrickValue[brickAddress + uint3(1)] = col;

    // face x
    col = float4(0);
    col += 0.25 * brickValue[1];
    col += 0.25 * brickValue[3];
    col += 0.25 * brickValue[5];
    col += 0.25 * brickValue[7];
    texBrickValue[brickAddress + uint3(2,1,1)] = col;

    // face neg x
    col = float4(0);
    col += 0.25 * brickValue[0];
    col += 0.25 * brickValue[2];
    col += 0.25 * brickValue[4];
    col += 0.25 * brickValue[6];
    texBrickValue[brickAddress + uint3(0,1,1)] = col;

      // Face Y
    col = float4(0);
    col += 0.25 * brickValue[2];
    col += 0.25 * brickValue[3];
    col += 0.25 * brickValue[6];
    col += 0.25 * brickValue[7];
    texBrickValue[brickAddress + uint3(1,2,1)] = col;

    // Face Y Neg
    col = float4(0);
    col += 0.25 * brickValue[0];
    col += 0.25 * brickValue[1];
    col += 0.25 * brickValue[4];
    col += 0.25 * brickValue[5];
    texBrickValue[brickAddress + uint3(1,0,1)] = col;

  
    // Face Z
    col = float4(0);
    col += 0.25 * brickValue[4];
    col += 0.25 * brickValue[5];
    col += 0.25 * brickValue[6];
    col += 0.25 * brickValue[7];
    texBrickValue[brickAddress + uint3(1,1,2)] = col;

    // Face Z Neg
    col = float4(0);
    col += 0.25 * brickValue[0];
    col += 0.25 * brickValue[1];
    col += 0.25 * brickValue[2];
    col += 0.25 * brickValue[3];
    texBrickValue[brickAddress + uint3(1,1,0)] = col;


    // Edges
    col = float4(0);
    col += 0.5 * brickValue[0];
    col += 0.5 * brickValue[1];
    texBrickValue[brickAddress + uint3(1,0,0)] = col;

    col = float4(0);
    col += 0.5 * brickValue[0];
    col += 0.5 * brickValue[2];
    texBrickValue[brickAddress + uint3(0,1,0)] = col;

    col = float4(0);
    col += 0.5 * brickValue[2];
    col += 0.5 * brickValue[3];
    texBrickValue[brickAddress + uint3(1,2,0)] = col;

    col = float4(0);
    col += 0.5 * brickValue[3];
    col += 0.5 * brickValue[1];
    texBrickValue[brickAddress + uint3(2,1,0)] = col;

    col = float4(0);
    col += 0.5 * brickValue[0];
    col += 0.5 * brickValue[4];
    texBrickValue[brickAddress + uint3(0,0,1)] = col;

    col = float4(0);
    col += 0.5 * brickValue[2];
    col += 0.5 * brickValue[6];
    texBrickValue[brickAddress + uint3(0,2,1)] = col;

    col = float4(0);
    col += 0.5 * brickValue[3];
    col += 0.5 * brickValue[7];
    texBrickValue[brickAddress + uint3(2,2,1)] = col;

    col = float4(0);
    col += 0.5 * brickValue[1];
    col += 0.5 * brickValue[5];
    texBrickValue[brickAddress + uint3(2,0,1)] = col;

    col = float4(0);
    col += 0.5 * brickValue[4];
    col += 0.5 * brickValue[6];
    texBrickValue[brickAddress + uint3(0,1,2)] = col;

    col = float4(0);
    col += 0.5 * brickValue[6];
    col += 0.5 * brickValue[7];
    texBrickValue[brickAddress + uint3(1,2,2)] = col;

    col = float4(0);
    col += 0.5 * brickValue[5];
    col += 0.5 * brickValue[7];
    texBrickValue[brickAddress + uint3(2,1,2)] = col;

    col = float4(0);
    col += 0.5 * brickValue[4];
    col += 0.5 * brickValue[5];
    texBrickValue[brickAddress + uint3(1,0,2)] = col;
}

float4 get_final_val(in float4 borderVal, in float4 neighbourBorderVal) {
    float4 col = 0.5 * (borderVal + neighbourBorderVal);
    return col;
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void border_transfer(uint3 DTid : SV_DispatchThreadID){
    uint32_t linearIndex = DTid.z * uDispathGroupSize.y * uDispathGroupSize.x + DTid.y * uDispathGroupSize.x + DTid.x;
    uint32_t levelStart = bufLevelAddress.Load((bufVoxelMeta.CurLevel) * 4);
    uint32_t levelNextStart = bufVoxelMeta.CurLevel <= (bufVoxelMeta.TotalLevel) ? bufLevelAddress.Load((bufVoxelMeta.CurLevel + 1) * 4) : 0;

    uint32_t nodeAddress = levelStart + linearIndex;
    if(nodeAddress >= levelNextStart) return;

    uint32_t neighbourAddress = get_child_index(bufNeighbourAddress.Load(nodeAddress * 4 * 8));
    if (neighbourAddress == 0) return; 

    uint3 brickAddr = uint_XYZ10_to_uint3(bufSvoNodeColor.Load(nodeAddress * 4 * 8));
    uint3 nBrickAddr = uint_XYZ10_to_uint3(bufSvoNodeColor.Load(neighbourAddress * 4));

    if(iBorderTransferAxis == AXIS_X) {
        for (int y = 0; y <= 2; ++y) {
            for (int z = 0; z <= 2; ++z) {
                uint3 offset = uint3(2,y,z);
                uint3 nOffset = uint3(0,y,z);
                float4 borderVal = texBrickValue[brickAddr + offset];
                float4 neighbourBorderVal = texBrickValue[nBrickAddr + nOffset];
                DeviceMemoryBarrier();
                float4 finalVal = get_final_val(borderVal, neighbourBorderVal);
                texBrickValue[brickAddr + offset] = finalVal;
                texBrickValue[nBrickAddr + nOffset] = finalVal;
            }
        }
    } else if(iBorderTransferAxis == AXIS_Y) {
        for (int x = 0; x <= 2; ++x) {
            for (int z = 0; z <= 2; ++z) {
                uint3 offset = uint3(x,2,z);
                uint3 nOffset = uint3(x,0,z);
                float4 borderVal = texBrickValue[brickAddr + offset];
                float4 neighbourBorderVal = texBrickValue[nBrickAddr + nOffset];
                DeviceMemoryBarrier();
                float4 finalVal = get_final_val(borderVal, neighbourBorderVal);
                texBrickValue[brickAddr + offset] = finalVal;
                texBrickValue[nBrickAddr + nOffset] = finalVal;
            }
        }
    } else if(iBorderTransferAxis == AXIS_Z) {
        for (int x = 0; x <= 2; ++x) {
            for (int y = 0; y <= 2; ++y) {
                uint3 offset = uint3(x,y,2);
                uint3 nOffset = uint3(x,y,0);
                float4 borderVal = texBrickValue[brickAddr + offset];
                float4 neighbourBorderVal = texBrickValue[nBrickAddr + nOffset];
                DeviceMemoryBarrier();

                float4 finalVal = get_final_val(borderVal, neighbourBorderVal);
                texBrickValue[brickAddr + offset] = finalVal;
                texBrickValue[nBrickAddr + nOffset] = finalVal;        
            }
        }
    }
}
