#pragma once
#include "Samples/SvoGi/Shaders/VoxelizationMeta.slangh"
#include "Samples/SvoGi/Shaders/VoxelizationTracing.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    VoxelizationMeta bufVoxelMeta;
    float2 fViewportDims;   
}

Texture3D<float4> texBrickValue;
SamplerState spTexSampler;
RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;


struct BrickTracingHitInfo : ITracingHitInfo {
    float4 Color = {};
    RayDesc Ray = {};

    [mutating]
    bool deal_if_hitted(uint32_t parentNode, uint32_t childShift, int32_t scale, Stack stack, float childScale, float t){
        uint32_t colorAddress = get_child_index(parentNode) * 8;
        uint32_t nodeData = bufSvoNodeColor.Load(colorAddress * 4);
        uint3 brickCoords = uint_XYZ10_to_uint3(nodeData);

        // load and uncompress
#ifdef USE_SAMPLER 

        float3 minPos = { 0 };
        float3 maxPos = { 0 };

        get_voxel_spatital_info(stack, scale, childShift, childScale, minPos, maxPos);
        float3 posTex = Ray.Origin + Ray.Direction * t;

        float tEnter = 0.f;
        float tLeave = 0.f;
        intersect_ray_with_AABB(Ray, minPos, maxPos, tEnter, tLeave);

        float3 childEnter = (posTex - minPos) / childScale;
        float3 childLeave = ((posTex + Ray.Direction * (tLeave - tEnter)) - minPos) / childScale;
        Color = ray_cast(Color, brickCoords, childEnter, childLeave, Ray.Direction, MAX_LEVEL - scale, childScale, bufVoxelMeta, texBrickValue, spTexSampler);
        bool hitted =  Color.w > 0.99f;
#else
        Color = texBrickValue[brickCoords + 2 * childOffsets[childShift]];
        bool hitted =  Color.w > 0.f;
#endif
        return hitted;
    }
}


float4 brick_main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * fViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, fViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, bufVoxelMeta.Min, bufVoxelMeta.Max);
    
    BrickTracingHitInfo hitInfo = {};
    hitInfo.Ray = ray;
    trace_sparse_voxel_ray(ray, bufSvoNodeNext, bufVoxelMeta.CurLevel, bufVoxelMeta.CellDim, hitInfo);

    return float4(hitInfo.Color.xyz, 1.0f);

}
