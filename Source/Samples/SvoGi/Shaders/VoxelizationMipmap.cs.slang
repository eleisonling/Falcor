#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"


cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
    uint3 uDispathGroupSize;
    int32_t iBorderTransferAxis;
}

RWByteAddressBuffer bufLevelAddress;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;

RWTexture3D<float4> texBrickValue;


static uint32_t childNext[8] = {};
static uint32_t childColor[8] = {};

void load_child_tile(in uint32_t tileAddress) {
    for(int32_t i = 0; i < 8; ++i) {
        childNext[i] = bufSvoNodeNext.Load((tileAddress + i) * 4);
        childColor[i] = bufSvoNodeColor.Load((tileAddress + i) * 4);
    } 
}


float4 get_color(in int3 pos) {
    int3 childPos = int3(round(float3(pos) / 4.0f));
    int32_t childIndex = childPos.x + 2 * childPos.y + 4 * childPos.z;
    int3 localPos = pos - 2 * childPos;
    uint3 childBrickAddress = uint_XYZ10_to_uint3(childColor[childIndex]);
    return texBrickValue[childBrickAddress + localPos];
}

float4 mipmap_isotropic(in int3 pos) {
    float4 col = float4(0);
    float weightSum = 0.0f;

    for(int32_t x = -1; x <= 1; ++x) {
        for(int32_t y = -1; y <= 1; ++y) {
            for(int32_t z = -1; z <= 1; ++z) {
                const int3 lookupPos = pos + int3(x,y,z);
                if (lookupPos.x >= 0 && lookupPos.y >= 0 && lookupPos.z >= 0 && lookupPos.x <= 4 && lookupPos.y <= 4 && lookupPos.z <= 4) {
                    const int32_t manhattanDist = abs(x) + abs(y) + abs(z);
                    const float weight = gaussianWeight[manhattanDist];
                    const float4 lookupColor = get_color(lookupPos);

                    col += weight * lookupColor;
                    weightSum += weight;
                }
            }
        }
    }

    return col / weightSum;
}


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void mipmap_center(uint3 DTid : SV_DispatchThreadID){
    uint32_t linearIndex = DTid.z * uDispathGroupSize.y * uDispathGroupSize.x + DTid.y * uDispathGroupSize.x + DTid.x;
    uint32_t levelStart = bufLevelAddress.Load((bufVoxelMeta.CurLevel) * 4);
    uint32_t levelNextStart = bufVoxelMeta.CurLevel <= (bufVoxelMeta.TotalLevel) ? bufLevelAddress.Load((bufVoxelMeta.CurLevel + 1) * 4) : 0;

    uint32_t nodeAddress = levelStart + linearIndex;
    if(nodeAddress >= levelNextStart) return;

    uint32_t nodeNextU = bufSvoNodeNext.Load(nodeAddress * 4 * 8);
    if(!is_valid_node(nodeNextU)) return;

    uint32_t brickAddressU = bufSvoNodeColor.Load(nodeAddress * 4 * 8);
    uint3 brickAddress = uint_XYZ10_to_uint3(brickAddressU);

    uint32_t childAddress = get_child_index(nodeNextU);
    load_child_tile(childAddress);

    float4 color = mipmap_isotropic(int3(2,2,2));
    DeviceMemoryBarrier();

    texBrickValue[brickAddress + uint3(1)] = color;
}
