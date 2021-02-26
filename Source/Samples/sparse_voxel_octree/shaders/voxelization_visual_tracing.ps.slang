#pragma once
#include "Samples/sparse_voxel_octree/shaders/voxelization_meta.slangh"
#include "Samples/sparse_voxel_octree/shaders/voxelization_tracing.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    voxelization_meta bufVoxelMeta;
    float2 fViewportDims;   
}

Texture3D<float4> texPackedAlbedo;
SamplerState spTexSampler;
RWByteAddressBuffer bufSvoNode;

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * fViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, fViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, bufVoxelMeta.CellSize, bufVoxelMeta.SvoDim, bufVoxelMeta.Min);

    tracing_result hitInfo = trace_sparse_voxel_ray(ray, texPackedAlbedo, bufSvoNode, bufVoxelMeta.TotalLevel, bufVoxelMeta.CellDim, spTexSampler);

    return hitInfo.Color;
}
