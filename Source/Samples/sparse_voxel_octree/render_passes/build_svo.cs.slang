#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"

cbuffer CB {
    svo_meta gSvoMeta;
}

Texture3D<uint32_t> gPackedAlbedo;
RWByteAddressBuffer gSvoNodeBuffer;

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

RWByteAddressBuffer gDivideIndirectArg;

uint32_t get_child_index(uint32_t nodeData) {
    return nodeData & (g_internalFlag - 1);
}

uint32_t set_child_index(uint32_t index) {
    return index | g_internalFlag;
}

bool is_valid_node(uint32_t nodeData) {
    return (nodeData & g_internalFlag) != 0;
}

[numthreads(g_tagThreads, g_tagThreads, g_tagThreads)]
void tag_node(uint3 g_threadId : SV_DispatchThreadID){

    if(g_threadId.x >= gSvoMeta.CellDim.x || g_threadId.y >= gSvoMeta.CellDim.y || g_threadId.z >= gSvoMeta.CellDim.z) return;
    uint3 cellIndex =  g_threadId;

    uint32_t packedColor = gPackedAlbedo[cellIndex];
    pixelCount = (packedColor & 0xFF000000) >> 24U;

    if(pixelCount == 0) return;

    uint32_t childID = 0;
    uint32_t curID = 0;
    uint3 svoCellDim = gSvoMeta.SvoDim;

    for(int32_t i = 0; i < gSvoMeta.CurLevel; ++i) {
        svoCellDim >>= 1;
        uint3 relative = step(svoCellDim, cellIndex);
        curID = childID + (relative.x | relative.y << 1 | relative.z << 2);
        childID = get_child_index(gSvoNodeBuffer.Load(curID * 4)) * 8;
        cellIndex -= svoCellDim * relative;
    }

    if(gSvoMeta.CurLevel == gSvoMeta.TotalLevel) {
        // svo node is same as the pixel cell node count
        gSvoNodeBuffer.Store(curID * 4, set_child_index(linearIndex));
    } else {
        gSvoNodeBuffer.InterlockedOr(curID * 4, g_internalFlag); 
    }
}


[numthreads(1, 1, 1)]
void caculate_divide_indirect_arg() {
    uint32_t preNodeNum = gDivideIndirectArg.Load(g_preNodeNum);
    uint32_t curNodeNum = gDivideIndirectArg.Load(g_curNodeNum);

    uint32_t num = (curNodeNum - preNodeNum) * 8;
    gDivideIndirectArg.Store(g_deltaNodeNum, num);

    num = (num + g_divideThreads - 1) / g_divideThreads;
    gDivideIndirectArg.Store(g_dispathX, num);
    gDivideIndirectArg.Store(g_dispathY, 1);
    gDivideIndirectArg.Store(g_dispathZ, 1);

    gDivideIndirectArg.Store(g_preNodeNum, curNodeNum);
    gDivideIndirectArg.Store(g_nodeOffset, preNodeNum * 8);
}


[numthreads(g_divideThreads, 1, 1)]
void sub_divide_node(uint3 g_threadId : SV_DispatchThreadID){
    if(g_threadId.x >= gDivideIndirectArg.Load(g_deltaNodeNum)) return;
    uint32_t index = g_threadId.x + gDivideIndirectArg.Load(g_nodeOffset);

    uint32_t nodeData = gSvoNodeBuffer.Load(index * 4);

    if(is_valid_node(nodeData) && get_child_index(nodeData) == 0) {
        uint32_t nodeNum = 0;
        gDivideIndirectArg.InterlockedAdd(g_curNodeNum, 1, nodeNum);

        gSvoNodeBuffer.Store(index * 4, set_child_index(nodeNum));
        uint32_t childNodeStart = nodeNum * 8;
        for (int32_t i = 0; i < 8; ++i) {
            uint32_t address = (childNodeStart + i) * 4;
            gSvoNodeBuffer.Store(address, 0);
        }
    }
}
