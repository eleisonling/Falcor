#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    float2 ViewportDims;   
}

RWByteAddressBuffer gDebugTracingData;

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * ViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, ViewportDims).toRayDesc();

    // debuging
    if(launchIndex.x == 0 && launchIndex.y == 0){
       gDebugTracingData.Store(0, asuint(ray.Origin.x)); 
       gDebugTracingData.Store(4, asuint(ray.Origin.y)); 
       gDebugTracingData.Store(8, asuint(ray.Origin.z)); 
       gDebugTracingData.Store(12, asuint(ray.Direction.x)); 
       gDebugTracingData.Store(16, asuint(ray.Direction.y)); 
       gDebugTracingData.Store(20, asuint(ray.Direction.z)); 
    }

    return {1, 0, 0, 1};
}
