#pragma once
#include "Samples/SvoGi/Shaders/VCT.slangh"

import Scene.Shading;
import Scene.Raster;

cbuffer PerFrameCB : register(b0) {
    float4x4 matShadowMatrix;
    int2 iPcfKernel;
    int2 iShadowMapDimension; 
    VoxelizationMeta bufVoxelMeta;

};


Texture2D<float>  texShadowMap;
SamplerComparisonState spPcfSampler;

RWByteAddressBuffer bufSvoNodeNext;
RWByteAddressBuffer bufSvoNodeColor;
Texture3D<float4> texBrickValue;
SamplerState spTexSampler;



float fixed_size_pcf(float2 texC, float depthRef) {

    float2 pixelSize = 1.0 / float2(iShadowMapDimension.xy);
    float res = 0;

    int2 halfKernelSize = iPcfKernel / 2;
    for(int i = -halfKernelSize.x; i <= halfKernelSize.x; i++) {
        for(int j = -halfKernelSize.y; j <= halfKernelSize.y; j++) {
            float2 sampleCrd = texC + float2(i, j) * pixelSize;
            res += texShadowMap.SampleCmpLevelZero(spPcfSampler, sampleCrd, depthRef);
        }
    }
    return res / (iPcfKernel.x * iPcfKernel.y);
}


float4 main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {
    float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir);
    float4 finalColor = {};
    float4 directColor = float4(0, 0, 0, 1);
    float4 tracingColor = {};
    float tracingOcclusion = 0.0f;

    for(int32_t i = 0; i < DIFFUSE_CONE_COUNT; i++) {
        RayDesc ray = {};
        ray.Origin = convert_world_to_svo_space(vOut.posW, bufVoxelMeta.Min, bufVoxelMeta.Max);
        ray.Direction = normalize(local_to_world(DIFFUSE_CONE_DIRECTIONS[i], sd.N, sd.T, sd.B));
        tracingColor += cone_tracing(ray, DIFFUSE_CONE_APETURE, bufVoxelMeta, bufSvoNodeNext, bufSvoNodeColor, texBrickValue, spTexSampler, tracingOcclusion);
    }
    tracingColor /= DIFFUSE_CONE_COUNT;

    for (int i = 0; i < gScene.getLightCount(); i++) {
        directColor.rgb += evalMaterial(sd, gScene.getLight(i), 1).color;
    }

    float4 lightposH = mul(float4(vOut.posW, 1.0f), matShadowMatrix);
    lightposH /= lightposH.w;

    float factor = fixed_size_pcf(lightposH.xy * float2(0.5, -0.5) + 0.5, lightposH.z);
    directColor = lerp(float4(0,0,0,1), directColor, factor);

#ifdef DUMP_AO 
    fianlColor = lerp(float4(0,0,0,1), float4(1,1,1,1), tracingColor.a)
#else
    finalColor = directColor;
#endif

    return finalColor;
}
