#pragma once

Texture2D<float3> g_colorTex;
SamplerState g_texSampler;

float3 apply_sRGB_curve(float3 x) {
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float4 main(float2 texC : TEXCOORD, float4 posS : SV_POSITION) : SV_TARGET  {
    float3 linearRGB = g_colorTex.SampleLevel(g_texSampler, texC, 0);
    float3 gammaRGB = apply_sRGB_curve(linearRGB);

    return float4(gammaRGB, 1.0f);
}
