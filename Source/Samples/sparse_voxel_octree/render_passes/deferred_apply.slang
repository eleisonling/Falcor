#pragma once
import Scene.Raster;

cbuffer CB {
    float4x4 g_shadow_matrix;
    int2 g_pcf_kernel_size;
    float2 g_screen_dimension;
    uint2 g_shadowmap_dimension;
}


Texture2D g_shadowMap;
Texture2D g_sceneColor;
Texture2D g_sceneDepth;

SamplerComparisonState g_pcfSampler;
SamplerState g_texSampler;

float4 load_position(float2 uv)
{
    float depth = g_sceneDepth.SampleLevel(g_texSampler, uv, 0).r;
    float2 ndc = 2 * uv - 1;
#ifdef FALCOR_D3D
    ndc.y = -ndc.y;
#endif
    float4 posW = mul(float4(ndc.x, ndc.y, depth, 1.f), gScene.camera.data.invViewProj);
    return posW / posW.w;
}

float fixed_size_pcf(float2 texC, float depthRef) {

    float2 pixelSize = 1.0 / float2(g_shadowmap_dimension.xy);
    float res = 0;

    int2 halfKernelSize = g_pcf_kernel_size / 2;
    for(int i = -halfKernelSize.x; i <= halfKernelSize.x; i++)
    {
        for(int j = -halfKernelSize.y; j <= halfKernelSize.y; j++)
        {
            float2 sampleCrd = texC + float2(i, j) * pixelSize;
            res += g_shadowMap.SampleCmpLevelZero(g_pcfSampler, sampleCrd, depthRef).r;
        }
    }
    return res / (g_pcf_kernel_size.x * g_pcf_kernel_size.y);
}

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET  {
    float4 posW = load_position(texC);
    float4 lightposH = mul(posW, g_shadow_matrix);
    lightposH /= lightposH.w;

    float factor = fixed_size_pcf(lightposH.xy * float2(0.5, -0.5) + 0.5, lightposH.z);
    float4 finalColor = g_sceneColor.Sample(g_texSampler, texC);
    finalColor = lerp(float4(0,0,0,1), finalColor, factor);

    return finalColor;
}
