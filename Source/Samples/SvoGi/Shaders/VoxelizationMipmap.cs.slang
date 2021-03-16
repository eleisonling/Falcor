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


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void mipmap_faces(uint3 DTid : SV_DispatchThreadID){
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

    float4 left = mipmap_isotropic(int3(0,2,2));
    float4 right = mipmap_isotropic(int3(4,2,2));
    float4 bottom = mipmap_isotropic(int3(2,0,2));
    float4 top = mipmap_isotropic(int3(2,4,2));
    float4 near = mipmap_isotropic(int3(2,2,0));
    float4 far = mipmap_isotropic(int3(2,2,4));

    DeviceMemoryBarrier();

    texBrickValue[brickAddress + uint3(0,1,1)] = left;
    texBrickValue[brickAddress + uint3(2,1,1)] = right;
    texBrickValue[brickAddress + uint3(1,0,1)] = bottom;
    texBrickValue[brickAddress + uint3(1,2,1)] = top;
    texBrickValue[brickAddress + uint3(1,1,0)] = near;
    texBrickValue[brickAddress + uint3(1,1,2)] = far;
}


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void mipmap_edges(uint3 DTid : SV_DispatchThreadID){
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

    float4 nearBottom = mipmap_isotropic(int3(2, 0, 0));
    float4 nearRight = mipmap_isotropic(int3(4, 2, 0));
    float4 nearTop = mipmap_isotropic(int3(2, 4, 0));
    float4 nearLeft = mipmap_isotropic(int3(0, 2, 0));
    float4 farBottom = mipmap_isotropic(int3(2, 0, 4));
    float4 farRight = mipmap_isotropic(int3(4, 2, 4));
    float4 farTop = mipmap_isotropic(int3(2, 4, 4));
    float4 farLeft = mipmap_isotropic(int3(0, 2, 4));
    float4 leftBottom = mipmap_isotropic(int3(0, 0, 2));
    float4 leftTop = mipmap_isotropic(int3(0, 4, 2));
    float4 rightBottom = mipmap_isotropic(int3(4, 0, 2));
    float4 rightTop = mipmap_isotropic(int3(4, 4, 2));

    DeviceMemoryBarrier();

    texBrickValue[brickAddress + int3(1,0,0)] = nearBottom;
    texBrickValue[brickAddress + int3(2,1,0)] = nearRight;
    texBrickValue[brickAddress + int3(1,2,0)] = nearTop;
    texBrickValue[brickAddress + int3(0,1,0)] = nearLeft;
    texBrickValue[brickAddress + int3(1,0,2)] = farBottom;
    texBrickValue[brickAddress + int3(2,1,2)] = farRight;
    texBrickValue[brickAddress + int3(1,2,2)] = farTop;
    texBrickValue[brickAddress + int3(0,1,2)] = farLeft;
    texBrickValue[brickAddress + int3(0,0,1)] = leftBottom;
    texBrickValue[brickAddress + int3(2,0,1)] = rightBottom;
    texBrickValue[brickAddress + int3(0,2,1)] = leftTop;
    texBrickValue[brickAddress + int3(2,2,1)] = rightTop;
}


[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void mipmap_corners(uint3 DTid : SV_DispatchThreadID){
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


    float4 nearRightTop = mipmap_isotropic(int3(4, 4, 0));
    float4 nearRightBottom = mipmap_isotropic(int3(4, 0, 0));
    float4 nearLeftTop = mipmap_isotropic(int3(0, 4, 0));
    float4 nearLeftBottom = mipmap_isotropic(int3(0, 0, 0));
    float4 farRightTop = mipmap_isotropic(int3(4, 4, 4));
    float4 farRightBottom = mipmap_isotropic(int3(4, 0, 4));
    float4 farLeftTop = mipmap_isotropic(int3(0, 4, 4));
    float4 farLeftBottom = mipmap_isotropic(int3(0, 0, 4));
  
    DeviceMemoryBarrier();
  
    texBrickValue[brickAddress + int3(2, 2, 0)] = nearRightTop;
    texBrickValue[brickAddress + int3(2, 0, 0)] = nearRightBottom;
    texBrickValue[brickAddress + int3(0, 2, 0)] = nearLeftTop;
    texBrickValue[brickAddress + int3(0, 0, 0)] = nearLeftBottom;
    texBrickValue[brickAddress + int3(2, 2, 2)] = farRightTop;
    texBrickValue[brickAddress + int3(2, 0, 2)] = farRightBottom;
    texBrickValue[brickAddress + int3(0, 2, 2)] = farLeftTop;
    texBrickValue[brickAddress + int3(0, 0, 2)] = farLeftBottom;
}
