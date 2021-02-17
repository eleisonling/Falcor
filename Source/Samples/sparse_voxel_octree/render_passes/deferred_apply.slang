#pragma once
import Scene.Raster;

cbuffer CB {
    float4x4 g_shadow_matrix;
    float2 g_pcf_kernel_size;
    float2 g_screen_dimension;
}


Texture2D g_shadowMap;
Texture2D g_sceneColor;
Texture2D g_sceneDepth;

SamplerComparisonState g_pcfSampler;
SamplerState g_texSampler;

float4 load_position(float2 uv)
{
    float depth = g_sceneDepth.SampleLevel(g_texSampler, uv, 0).r;
    float2 invSize = 1.0f / g_screen_dimension.xy;
    // Recompute position by unprojecting the depth stored in the z-buffer 
    float2 ndc = 2 * uv + invSize - 1;
#ifdef FALCOR_D3D
    ndc.y = -ndc.y;
#endif
    float4 posW = mul(float4(ndc.x, ndc.y, depth, 1.f), gScene.camera.data.invViewProj);
    return posW / posW.w;
}

float fixed_size_pcf(float2 texC, float depthRef) {
    float res = 0;
    res = g_shadowMap.SampleCmpLevelZero(g_pcfSampler, texC, depthRef).r;
    return res;
}

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET  {
    float4 posW = load_position(texC);
    float4 lightposH = mul(posW, g_shadow_matrix);
    lightposH.z /= lightposH.w;

    float factor = fixed_size_pcf(lightposH.xy, lightposH.z);
    float4 finalColor = g_sceneColor.Sample(g_texSampler, texC);
    finalColor = lerp(finalColor, float4(0,0,0,0), factor);

    return finalColor;
}
