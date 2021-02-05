#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"

cbuffer CB {
    svo_meta gSvoMeta;
}
RWByteAddressBuffer gPixelPacked;
RWByteAddressBuffer gSvoNodeBuffer;

// 0, 4, 8 dispath group
// 12 preNode num
// 16 curNode num
// 20 deltaNode num
// 24 node offset
constexpr uint32 g_dispathX = 0;
constexpr uint32 g_dispathY = 4;
constexpr uint32 g_dispathZ = 8;
constexpr uint32 g_preNodeNum = 12;
constexpr uint32 g_curNodeNum = 16;
constexpr uint32 g_deltaNodeNum = 20;
constexpr uint32 g_nodeOffset = 24;

RWByteAddressBuffer gDivideIndirectArg;

uint32_t get_child_index(uint32_t nodeData) {
    return nodeData & (g_internalFlag - 1);
}

uint32_t set_child_index(uint32_t index) {
    return index | g_internalFlag;
}

[numthreads(g_avgThreads, 1, 1)]
void tag_node(uint3 g_threadId : SV_DispatchThreadID){

    if(g_threadId.x >= gSvoMeta.CellNum) return;
    uint3 cellIndex = extract_index_form_linear(g_threadIdx.x, gSvoMeta.CellDim);

    uint32_t address = g_threadIdx.x * 4 + 16;
    if(gPixelPacked.Load(address) == 0) return;

    uint32_t childID = 0;
    uint32_t curID = 0;
    uint3 svoCellDim = gSvoMeta.SvoDim;

    for(int32_t i = 0; i < gSvoMeta.CurLevel; ++i) {
        svoCellDim >>= 1;
        uint3 relative = step(svoCellDim, cellIndex);
        curID = childID + (relative.x | relative.y << 1 | relative.z << 2);
        childID = get_child_index(gSvoNodeBuffer.Load(curID * 4)) * 8;
        cellIndex -= svoCellDim * relativeID;
    }

    if(gSvoMeta.CurLevel == gSvoMeta.TotalLevel) {
        // svo node is same as the pixel cell node count
        gSvoNodeBuffer.Store(curID * 4, g_threadIndex.x);
    } else {
        gSvoNodeBuffer.AtomicOr(curID * 4, g_internalFlag); 
    }
}


[numthreads(1, 1, 1)]
void caculate_divide_indirect_arg() {
    uint32_t preNodeNum = gDivideIndirectArg.Load(g_preNodeNum);
    uint32_t curNodeNum = gDivideIndirectArg.Load(g_curNodeNum);

    uint32_t num = (curNodeNum - preNodeNum) * 8;
    gDiveIndirectArg.Store(g_deltaNodeNum, num);

    num = (num + g_divideThreads - 1) / g_divideThreads;
    gDivideIndirectArg.Store(g_dispathX, num);
    gDivideIndirectArg.Store(g_dispathY, 1);
    gDivideIndirectArg.Store(g_dispathZ, 1);

    gDivideIndirectArg.Store(g_preNodeNum, curNodeNum);
    gDivideIndirectArg.Store(g_nodeOffset, preNodeNum * 8);
}


[numthreads(g_divideThreads, 1, 1)]
void sub_divide_node(uint3 g_threadId : SV_DispatchThreadID){
    if(g_threadId.x >= gDiveIndirectArg.Load(g_deltaNodeNum)) return;
    uint32_t index += gDiveIndirectArg.Load(g_nodeOffset)

    uint32_t nodeData = gSvoNodeBuffer.Load(index * 4);

    if(is_valid_node(nodeData) && get_child_node(nodeData) == 0) {
        uint32_t nodeNum = 0;
        gDivideIndirectArg.InterlockedAdd(g_curNodeNum, 1, nodeNum);

        gSvoNodeBuffer.Store(nodeNum * 4, set_child_index(nodeNum));
        uint32_t childNodeStart = nodeNum * 8;
        for (int32_t i = 0; i < 8; ++i) {
            uint32_t address = (childNodeStart + i) * 4;
            gSvoNodeBuffer.Store(address, 0);
        }
    }
}
