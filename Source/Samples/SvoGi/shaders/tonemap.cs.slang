#pragma once
#include "Samples/SvoGi/shaders/post_effects.slangh"

StructuredBuffer<exposure_meta> bufExposure;
Texture2D<float3> texBloom;
RWTexture2D<float3> texColorRW;
SamplerState spTexSampler;

cbuffer CB {
    float2 g_recpBuferDim;
    float g_bloomStrength;
}

float3 tonemap_ACES(float3 hdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return saturate((hdr * (A * hdr + B)) / (hdr * (C * hdr + D) + E));
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    float2 texCoord = (DTid.xy + 0.5) * g_recpBuferDim;
    float3 hdrColor = texColorRW[DTid.xy];
    hdrColor += g_bloomStrength * texBloom.SampleLevel(spTexSampler, texCoord, 0);
    hdrColor *= bufExposure[0].Exposure;
    float3 sdrColor = tonemap_ACES(hdrColor);
    texColorRW[DTid.xy] = sdrColor;
}
