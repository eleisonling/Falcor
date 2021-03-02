#pragma once

#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"

 RWByteAddressBuffer bufAtomicAndIndirect;


[numthreads(1, 1, 1)]
void make_indirect_frag_pos_arg() {
    uint32_t fragPos = bufAtomicAndIndirect.Load(ATOM_FRAG_NEXT * 4);
    uint3 threads = extract_index_form_linear(fragPos, uint3(DIVIDE_THREAD_SIZE));
    uint3 group = (threads + uint3(COMMON_THREAD_SIZE - 1)) / uint3(COMMON_THREAD_SIZE);
    bufAtomicAndIndirect.Store3(FRAG_NEXT_INDIRECT * 4, group);
}

[numthreads(1, 1, 1)]
void make_indirect_frag_node_arg() {
    uint32_t nodePos = bufAtomicAndIndirect.Load(ATOM_NODE_NEXT * 4);
    uint3 threads = extract_index_form_linear(nodePos, uint3(DIVIDE_THREAD_SIZE));
    uint3 group = (threads + uint3(COMMON_THREAD_SIZE - 1)) / uint3(COMMON_THREAD_SIZE);
    group = max(group, uint3(1));
    bufAtomicAndIndirect.Store3(NODE_NEXT_INDIRECT * 4, group);
}


