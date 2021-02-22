#pragma once

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
cbuffer CB {
    voxel_meta gVoxelMeta;
}

Texture3D<uint32_t> gPackedSrcData;
RWTexture3D<uint32_t> gPackedDstData;

static const uint3 OFFSETS[] = 
{
	uint3(1, 1, 1),
	uint3(1, 1, 0),
	uint3(1, 0, 1),
	uint3(1, 0, 0),
	uint3(0, 1, 1),
	uint3(0, 1, 0),
	uint3(0, 0, 1),
	uint3(0, 0, 0)
};

uint32_t get_avg(uint3 pos)
{
    float4 result = {};

	for(int i = 0; i < 8; i++)
	{
		uint32_t uPixel = gPackedSrcData[pos + OFFSETS[i]];
        result += uint32_to_RGBA8_255(uPixel);
	}
    
    result /= 8.0f;
    return RGBA8_255_to_uint32(result);
}

[numthreads(g_avgThreads, g_avgThreads, g_avgThreads)]
void gen_packed_mip(uint3 g_threadId : SV_DispatchThreadID){
    if(g_threadId.x >= gVoxelMeta.CellDim.x || g_threadId.y >= gVoxelMeta.CellDim.y || g_threadId.z >= gVoxelMeta.CellDim.z) return;

    uint3 readPos = g_threadId * 2;
    uint32_t mipAvg = get_avg(readPos);
    gPackedDstData[g_threadId] = mipAvg;
}
