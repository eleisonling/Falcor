#pragma once
#include "Samples/sparse_voxel_octree/shaders/post_effects.slangh"

StructuredBuffer<exposure_meta> g_exposure;
Texture2D<float3> g_bloom;
RWTexture2D<float3> g_colorRW;
SamplerState g_texSampler;

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
    float3 hdrColor = g_colorRW[DTid.xy];
    hdrColor += g_bloomStrength * g_bloom.SampleLevel(g_texSampler, texCoord, 0);
    hdrColor *= g_exposure[0].Exposure;
    float3 sdrColor = tonemap_ACES(hdrColor);
    g_colorRW[DTid.xy] = sdrColor;
}
