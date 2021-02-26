#pragma once
#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
#include "Samples/sparse_voxel_octree/render_passes/svo_tracing.slangh"
import Scene.Shading;
import Scene.Raytracing;

cbuffer CB {
    svo_meta gSvoMeta;
    float2 ViewportDims;   
    float gMip;
}

Texture3D<float4> gPackedAlbedo;
SamplerState spTexSampler;
RWByteAddressBuffer gSvoNodeBuffer;

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET {
    uint2 launchIndex = uint2(texC.xy * ViewportDims);
    RayDesc ray = gScene.camera.computeRayPinhole(launchIndex.xy, ViewportDims).toRayDesc();
    ray.Origin = convert_world_to_svo_space(ray.Origin, gSvoMeta.CellSize, gSvoMeta.SvoDim, gSvoMeta.Min);

    tracing_result hitInfo = trace_sparse_voxel_ray(ray, gPackedAlbedo, gSvoNodeBuffer, gSvoMeta.TotalLevel, gSvoMeta.CellDim, gMip, spTexSampler);

    return hitInfo.Color;
}
