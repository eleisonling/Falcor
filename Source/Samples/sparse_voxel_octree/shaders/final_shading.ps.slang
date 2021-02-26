#pragma once

import Scene.Shading;
import Scene.Raster;

cbuffer PerFrameCB : register(b0) {
    float4x4 matShadowMatrix;
    int2 iPcfKernel;
    int2 iShadowMapDimension; 
};

Texture2D<float>  texShadowMap;
SamplerComparisonState spPcfSampler;

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
    float4 directColor = float4(0, 0, 0, 1);

    for (int i = 0; i < gScene.getLightCount(); i++) {
        directColor.rgb += evalMaterial(sd, gScene.getLight(i), 1).color;
    }

    float4 lightposH = mul(float4(vOut.posW, 1.0f), matShadowMatrix);
    lightposH /= lightposH.w;

    float factor = fixed_size_pcf(lightposH.xy * float2(0.5, -0.5) + 0.5, lightposH.z);
    directColor = lerp(float4(0,0,0,1), directColor, factor);

    return directColor;
}
