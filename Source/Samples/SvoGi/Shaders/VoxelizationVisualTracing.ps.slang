#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
    float2 fViewportDims;   
}

Texture3D<float4> texVoxelValue;
Texture3D<float4> texBrickValue;
SamplerState spTexSampler;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;

struct VoxelTracingHitInfo : ITracingHitInfo {
    float4 Color = {};

    [mutating]
    bool deal_if_hitted(uint32_t parent, uint32_t childShift){
        uint32_t nodeData = 0;
        get_node_info(parent, childShift, nodeData, bufSvoNodeNext);
        uint32_t linearAddress = get_child_index(nodeData);
        uint3 cellIndex = extract_index_form_linear(linearAddress, bufVoxelMeta.CellDim);

        // load and uncompress
#ifdef USE_SAMPLER 
        float3 cellTex = float3(cellIndex) / float3(bufVoxelMeta.CellDim);
        Color = texVoxelValue.SampleLevel(spTexSampler, cellTex, 0);
#else
        Color = texVoxelValue[cellIndex];
#endif
        bool hitted =  Color.w > 0.f;
        return hitted;
    }
};


float4 voxel_main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * fViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, fViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, bufVoxelMeta.Min, bufVoxelMeta.Max);

    VoxelTracingHitInfo hitInfo = {};
    trace_sparse_voxel_ray(ray, bufSvoNodeNext, bufVoxelMeta.TotalLevel, bufVoxelMeta.CellDim, hitInfo);

    return float4(hitInfo.Color.xyz, 1.0f);
}


struct BrickTracingHitInfo : ITracingHitInfo {
    float4 Color = {};

    [mutating]
    bool deal_if_hitted(uint32_t parent, uint32_t childShift){
        uint32_t nodeData = 0;
        get_node_info(parent, childShift, nodeData, bufSvoNodeColor);
        uint3 brickCoords = uint_XYZ10_to_uint3(nodeData);

        // load and uncompress
#ifdef USE_SAMPLER 
        float3 cellTex = float3(brickCoords + uint3(1)) / float3(bufVoxelMeta.BrickPoolResolution);
        Color = texBrickValue.SampleLevel(spTexSampler, cellTex, 0);
#else
        Color = texBrickValue[brickCoords + 2 * childOffsets[childShift]];
#endif
        bool hitted =  Color.w > 0.f;
        return hitted;
    }
}


float4 brick_main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * fViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, fViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, bufVoxelMeta.Min, bufVoxelMeta.Max);
    
    BrickTracingHitInfo hitInfo = {};
    trace_sparse_voxel_ray(ray, bufSvoNodeNext, bufVoxelMeta.TotalLevel, bufVoxelMeta.CellDim, hitInfo);

    return float4(hitInfo.Color.xyz, 1.0f);

}
