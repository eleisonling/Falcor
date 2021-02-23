#pragma once

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
cbuffer CB {
    voxel_meta gVoxelMeta;
    uint32_t gMip;
}

Texture3D<float4> gPackedSrcData;
RWTexture3D<float4> gPackedDstData;

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

float4 get_avg(uint3 pos)
{
    float4 result = {};

	for(int i = 0; i < 8; i++)
	{
		result += gPackedSrcData[pos + OFFSETS[i]];
	}
    return result;
}

[numthreads(g_avgThreads, g_avgThreads, g_avgThreads)]
void gen_packed_mip(uint3 g_threadId : SV_DispatchThreadID){

    if(any(g_threadId >= (gVoxelMeta.CellDim >> gMip))) return;

    uint3 readPos = g_threadId * 2;
    float4 mipAvg = get_avg(readPos);
    gPackedDstData[g_threadId] = mipAvg;
}
