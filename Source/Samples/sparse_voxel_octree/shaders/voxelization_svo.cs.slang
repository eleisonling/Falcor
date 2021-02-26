#pragma once
#include "Samples/sparse_voxel_octree/shaders/voxelization_meta.slangh"

cbuffer CB {
    voxelization_meta bufVoxelMeta;
}

Texture3D<float4> texPackedAlbedo;
RWByteAddressBuffer bufSvoNode;

// 0, 4, 8 dispath group
// 12 preNode num
// 16 curNode num
// 20 deltaNode num
// 24 node offset
static constexpr uint32_t g_dispathX = 0;
static constexpr uint32_t g_dispathY = 4;
static constexpr uint32_t g_dispathZ = 8;
static constexpr uint32_t g_preNodeNum = 12;
static constexpr uint32_t g_curNodeNum = 16;
static constexpr uint32_t g_deltaNodeNum = 20;
static constexpr uint32_t g_nodeOffset = 24;

RWByteAddressBuffer bufDivideIndirectArg;

uint32_t get_child_index(uint32_t nodeData) {
    return nodeData & (INTERNAL_FLAG - 1);
}

uint32_t set_child_index(uint32_t index) {
    return index | INTERNAL_FLAG;
}

bool is_valid_node(uint32_t nodeData) {
    return (nodeData & INTERNAL_FLAG) != 0;
}

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void tag_node(uint3 DTid : SV_DispatchThreadID){

    if(DTid.x >= bufVoxelMeta.CellDim.x || DTid.y >= bufVoxelMeta.CellDim.y || DTid.z >= bufVoxelMeta.CellDim.z) return;
    uint3 cellIndex =  DTid;
    uint32_t linearIndex = DTid.z * bufVoxelMeta.CellDim.y * bufVoxelMeta.CellDim.x + DTid.y * bufVoxelMeta.CellDim.x + DTid.x;

    float4 packedColor = texPackedAlbedo[cellIndex];
    if(packedColor.a == 0.f) return;

    uint32_t childID = 0;
    uint32_t curID = 0;
    uint3 svoCellDim = bufVoxelMeta.SvoDim;

    for(int32_t i = 0; i < bufVoxelMeta.CurLevel; ++i) {
        svoCellDim >>= 1;
        uint3 relative = step(svoCellDim, cellIndex);
        curID = childID + (relative.x | relative.y << 1 | relative.z << 2);
        childID = get_child_index(bufSvoNode.Load(curID * 4)) * 8;
        cellIndex -= svoCellDim * relative;
    }

    if(bufVoxelMeta.CurLevel == bufVoxelMeta.TotalLevel) {
        // svo node is same as the pixel cell node count
        bufSvoNode.Store(curID * 4, set_child_index(linearIndex));
    } else {
        bufSvoNode.InterlockedOr(curID * 4, INTERNAL_FLAG); 
    }
}


[numthreads(1, 1, 1)]
void caculate_divide_indirect_arg() {
    uint32_t preNodeNum = bufDivideIndirectArg.Load(g_preNodeNum);
    uint32_t curNodeNum = bufDivideIndirectArg.Load(g_curNodeNum);

    uint32_t num = (curNodeNum - preNodeNum) * 8;
    bufDivideIndirectArg.Store(g_deltaNodeNum, num);

    num = (num + COMMON_THREAD_SIZE - 1) / COMMON_THREAD_SIZE;
    bufDivideIndirectArg.Store(g_dispathX, num);
    bufDivideIndirectArg.Store(g_dispathY, 1);
    bufDivideIndirectArg.Store(g_dispathZ, 1);

    bufDivideIndirectArg.Store(g_preNodeNum, curNodeNum);
    bufDivideIndirectArg.Store(g_nodeOffset, preNodeNum * 8);
}


[numthreads(COMMON_THREAD_SIZE, 1, 1)]
void sub_divide_node(uint3 DTid : SV_DispatchThreadID){
    if(DTid.x >= bufDivideIndirectArg.Load(g_deltaNodeNum)) return;
    uint32_t index = DTid.x + bufDivideIndirectArg.Load(g_nodeOffset);

    uint32_t nodeData = bufSvoNode.Load(index * 4);

    if(is_valid_node(nodeData) && get_child_index(nodeData) == 0) {
        uint32_t nodeNum = 0;
        bufDivideIndirectArg.InterlockedAdd(g_curNodeNum, 1, nodeNum);

        bufSvoNode.Store(index * 4, set_child_index(nodeNum));
        uint32_t childNodeStart = nodeNum * 8;
        for (int32_t i = 0; i < 8; ++i) {
            uint32_t address = (childNodeStart + i) * 4;
            bufSvoNode.Store(address, 0);
        }
    }
}
