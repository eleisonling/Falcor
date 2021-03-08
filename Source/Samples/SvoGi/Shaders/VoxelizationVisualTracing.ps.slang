#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
    float2 fViewportDims;   
}

Texture3D<float4> texPackedAlbedo;
SamplerState spTexSampler;
RWByteAddressBuffer bufSvoNode;

struct VoxelTracingHitInfo : ITracingHittedInfo {
    float4 Color = {};

    [mutating]
    bool deal_hitted(uint32_t nodeData){
        uint32_t linearAddress = get_child_index(nodeData);
        uint3 cellIndex = extract_index_form_linear(linearAddress, bufVoxelMeta.CellDim);

        // load and uncompress
        Color = texPackedAlbedo[cellIndex];
        bool hitted =  Color.w > 0.f;
        return hitted;
    }
};


float4 voxel_main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * fViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, fViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, bufVoxelMeta.Min, bufVoxelMeta.Max);

    VoxelTracingHitInfo hitInfo = {};
    trace_sparse_voxel_ray(ray, bufSvoNode, bufVoxelMeta.TotalLevel, bufVoxelMeta.CellDim, hitInfo);

    return float4(hitInfo.Color.xyz, 1.0f);
}
