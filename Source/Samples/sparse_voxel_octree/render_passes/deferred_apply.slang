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

float4 load_position(float2 uv)
{
    float4 pos;
    pos.x = uv.x * 2.0f - 1.0f;
    pos.y = (1.0f - uv.y) * 2.0f - 1.0f;
#ifdef FALCOR_VK
    // NDC space is inverted
    pos.y = -pos.y;
#endif
    pos.z = g_sceneDepth.SampleLevel(g_texSampler, uv, 0).r;
    pos.w = 1.0f;

    float4 posW = mul(pos, gScene.camera.data.invViewProj);
    posW /= posW.w;

    return posW;
}

float fixed_size_pcf(float2 texC, float depthRef) {
    float res = 0;
    res = g_shadowMap.SampleCmpLevelZero(g_pcfSampler, texC, depthRef).r;
    return res;
}

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET  {
    float4 posW = load_position(texC);
    float4 lightposH = mul(posW, g_light_view_proj);

    float d = g_shadowMap.SampleLevel(g_texSampler, texC, 0).r;

    float factor = fixed_size_pcf(texC, lightposH.z);

    float4 finalColor = g_sceneColor.Sample(g_texSampler, texC);
    finalColor = lerp(finalColor, float4(0,0,0,0), factor);

    return float4(lightposH.z, 0,0,0);
}
