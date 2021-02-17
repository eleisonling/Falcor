#pragma once
import Scene.Raster;

cbuffer CB {
    float4x4 g_light_view_proj;
    float2 g_pcf_kernel_size;
    float2 g_screen_dimension;
}


Texture2D g_shadowMap;
Texture2D g_sceneColor;
Texture2D g_sceneDepth;

SamplerComparisonState g_pcfSampler;
SamplerState g_texSampler;

float4 load_position(float2 UV, float depth)
{
    float2 ndc = 2 * UV - 1;
#ifdef FALCOR_D3D
    ndc.y = -ndc.y;
#endif
    float4 wsPos = mul(float4(ndc.x, ndc.y, depth, 1.f), gScene.camera.data.invViewProj);
    return wsPos / wsPos.w;
}

float fixed_size_pcf(float2 texC, float depthRef) {
    float res = 0;
    res = g_shadowMap.SampleCmpLevelZero(g_pcfSampler, texC, depthRef).r;
    return res;
}

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET  {
    float depth = g_sceneDepth.Sample(g_texSampler, texC).x;
    float4 posW = load_position(texC, depth);
    float4 lightposH = mul(posW, g_light_view_proj);
    lightposH /= lightposH.w;
    float factor = fixed_size_pcf(texC, lightposH.z);

    float4 finalColor = g_sceneColor.Sample(g_texSampler, texC);
    finalColor = lerp(finalColor, float4(0,0,0,0), factor);

    return float4(finalColor);
}
