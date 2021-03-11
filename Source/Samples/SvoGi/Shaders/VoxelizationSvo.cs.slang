#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
}

RWByteAddressBuffer bufSvoNode;
RWByteAddressBuffer bufAtomicAndIndirect;
RWByteAddressBuffer bufFragPosition;
RWByteAddressBuffer bufLevelAddress;

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

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void tag_node(uint3 DTid : SV_DispatchThreadID){

    uint32_t fragPosNum = bufAtomicAndIndirect.Load(ATOM_FRAG_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(FRAG_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= fragPosNum) return;

    uint32_t cellIndexU =  bufFragPosition.Load(linearIndex * 4);
    uint3 cellIndex = uint_XYZ10_to_uint3(cellIndexU);

    uint32_t childID = 0;
    uint32_t curID = 0;
    uint3 svoCellDim = bufVoxelMeta.CellDim;

    for(int32_t i = 0; i < bufVoxelMeta.CurLevel; ++i) {
        svoCellDim >>= 1;
        uint3 relative = step(svoCellDim, cellIndex);
        childID = get_child_index(bufSvoNode.Load(curID * 4)) * 8;
        curID = childID + (relative.x | relative.y << 1 | relative.z << 2);
        cellIndex -= svoCellDim * relative;
    }
    bufSvoNode.InterlockedOr(curID * 4, INTERNAL_FLAG); 
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
        bufAtomicAndIndirect.InterlockedAdd(ATOM_NODE_NEXT * 4, 1);
        bufSvoNode.Store(index * 4, set_child_index(nodeNum));
        bufLevelAddress.InterlockedMin(bufVoxelMeta.CurLevel * 4, nodeNum);
        if (bufVoxelMeta.CurLevel == bufVoxelMeta.TotalLevel - 1) {
             bufLevelAddress.InterlockedMax((bufVoxelMeta.CurLevel + 1) * 4, (nodeNum + 1));
        }
    }
}

RWByteAddressBuffer bufNeighbour_X;
RWByteAddressBuffer bufNeighbour_Y;
RWByteAddressBuffer bufNeighbour_Z;

[numthreads(COMMON_THREAD_SIZE, COMMON_THREAD_SIZE, COMMON_THREAD_SIZE)]
void create_neighbours(uint3 DTid : SV_DispatchThreadID){
    uint32_t fragPosNum = bufAtomicAndIndirect.Load(ATOM_FRAG_NEXT * 4);
    uint3 groupSize = bufAtomicAndIndirect.Load3(FRAG_NEXT_INDIRECT * 4) * uint3(COMMON_THREAD_SIZE);
    uint32_t linearIndex = DTid.z * groupSize.y * groupSize.x + DTid.y * groupSize.x + DTid.x;
    if (linearIndex >= fragPosNum) return;

    uint32_t cellIndexU =  bufFragPosition.Load(linearIndex * 4);
    uint3 cellIndex = uint_XYZ10_to_uint3(cellIndexU);

    uint32_t nodeLevel = bufVoxelMeta.CurLevel;
    uint3 relative = uint3(0);
    uint3 stepResolution = bufVoxelMeta.CellDim;
    uint32_t nodeAddress = traverse_return_cur(cellIndex, stepResolution, bufVoxelMeta.CurLevel + 1, bufSvoNode, nodeLevel, relative);
    uint32_t nodeData = bufSvoNode.Load(nodeAddress * 4);
    if(!is_valid_node(nodeData)) return;
    uint32_t neighbourAddress = get_child_index(nodeData);

    uint32_t nX = 0;
    uint32_t nY = 0;
    uint32_t nZ = 0;

    uint32_t neighbourLevel = bufVoxelMeta.CurLevel;
    if(relative.x == 0) {
        uint32_t neighbourXAddr = traverse_simple(cellIndex + uint3(stepResolution.x, 0, 0), bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNode, neighbourLevel);
        if (nodeLevel != neighbourLevel) {
            nX = 0;
        } else  {
            nX = get_child_index(bufSvoNode.Load(neighbourXAddr * 4));
        }
    }

    neighbourLevel = bufVoxelMeta.CurLevel;
    if(relative.y == 0) {
        uint32_t neighbourYAddr = traverse_simple(cellIndex + uint3(0, stepResolution.y, 0), bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNode, neighbourLevel);
        if (nodeLevel != neighbourLevel) {
            nY = 0;
        } else {
            nY = get_child_index(bufSvoNode.Load(neighbourYAddr * 4));
        }
    }

    neighbourLevel = bufVoxelMeta.CurLevel;
    if(relative.z == 0) {
        uint32_t neighbourZAddr = traverse_simple(cellIndex + uint3(0, 0, stepResolution.z), bufVoxelMeta.CellDim, bufVoxelMeta.TotalLevel, bufSvoNode, neighbourLevel);
        if (nodeLevel != neighbourLevel) {
            nZ = 0;
        } else {
            nZ = get_child_index(bufSvoNode.Load(neighbourZAddr * 4));
        }
    }

    bufNeighbour_X.Store(neighbourAddress * 4, set_child_index(nX));
    bufNeighbour_Y.Store(neighbourAddress * 4, set_child_index(nY));
    bufNeighbour_Z.Store(neighbourAddress * 4, set_child_index(nZ));
}
