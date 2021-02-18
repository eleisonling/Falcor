#pragma once
#include "Samples/sparse_voxel_octree/render_passes/post_effects.slangh"

SamplerState g_texSampler;
Texture2D g_sourceTex;
StructuredBuffer<exposure_meta> g_exposure;
RWTexture2D<float3> g_bloomResult;
RWTexture2D<uint32_t> g_lumaResult;

cbuffer CB {
    float2 g_inverseOutputSize;
    float g_bloomThreshold;
}

[numthreads(8, 8, 1)]
void extract_and_downsample(uint3 g_threadId : SV_DispatchThreadID){
    float2 uv = (g_threadId.xy + 0.5f) * g_inverseOutputSize;
    float2 offset = g_inverseOutputSize * 0.25f;

    float3 color1 = g_sourceTex.SampleLevel(g_texSampler, uv + float2(-offset.x, -offset.y), 0).rgb;
    float3 color2 = g_sourceTex.SampleLevel(g_texSampler, uv + float2( offset.x, -offset.y), 0).rgb;
    float3 color3 = g_sourceTex.SampleLevel(g_texSampler, uv + float2(-offset.x,  offset.y), 0).rgb;
    float3 color4 = g_sourceTex.SampleLevel(g_texSampler, uv + float2( offset.x,  offset.y), 0).rgb;

    float luma1 = rgb_to_luminance(color1);
    float luma2 = rgb_to_luminance(color2);
    float luma3 = rgb_to_luminance(color3);
    float luma4 = rgb_to_luminance(color4);

    const float kSmallEpsilon = 0.001f;

    float scaledThreshold = g_bloomThreshold * g_exposure[0].InvExposure;

    color1 *= max(kSmallEpsilon, luma1 - scaledThreshold) / (luma1 + kSmallEpsilon);
    color2 *= max(kSmallEpsilon, luma2 - scaledThreshold) / (luma2 + kSmallEpsilon);
    color3 *= max(kSmallEpsilon, luma3 - scaledThreshold) / (luma3 + kSmallEpsilon);
    color4 *= max(kSmallEpsilon, luma4 - scaledThreshold) / (luma4 + kSmallEpsilon);

    const float kShimmerFilterInverseStrength = 1.0f;
    float weight1 = 1.0f / (luma1 + kShimmerFilterInverseStrength);
    float weight2 = 1.0f / (luma2 + kShimmerFilterInverseStrength);
    float weight3 = 1.0f / (luma3 + kShimmerFilterInverseStrength);
    float weight4 = 1.0f / (luma4 + kShimmerFilterInverseStrength);
    float weightSum = weight1 + weight2 + weight3 + weight4;

    g_bloomResult[g_threadId.xy] = (color1 * weight1 + color2 * weight2 + color3 * weight3 + color4 * weight4) / weightSum;

    float luma = (luma1 + luma2 + luma3 + luma4) * 0.25f;
    if(luma == 0.0f) {
        g_lumaResult[g_threadId.xy] = 0;
    } else {
        const float minLog = g_exposure[0].MinLog;
        const float recpLogRange = g_exposure[0].InvDeltaLog;
        float logLuma = saturate((log2(luma) - minLog) * recpLogRange);
        g_lumaResult[g_threadId.xy] = logLuma * 254.0f + 1.0f;
    }
}


Texture2D<float3> g_bloomBuf;
RWTexture2D<float3> g_result1;
RWTexture2D<float3> g_result2;
RWTexture2D<float3> g_result3;
RWTexture2D<float3> g_result4;

groupshared float3 g_tile[64]; //8x8

[numthreads(8, 8, 1)]
void down_sample(uint32_t groupId : SV_GroupIndex, uint3 g_threadId : SV_DispatchThreadID){
    uint32_t parity = g_threadId.x | g_threadId.y;

    float2 centerUV = (float2(g_threadId.xy) * 2.0f + 1.0f) * g_inverseOutputSize;
    float3 avgPixel = g_bloomBuf.SampleLevel(g_texSampler, centerUV, 0.0f);
    g_tile[groupId] = avgPixel;
    g_result1[g_threadId.xy] = avgPixel;

    GroupMemoryBarrierWithGroupSync();

    if((parity & 1) == 0) {
        avgPixel = 0.25f * (avgPixel + g_tile[groupId + 1] + g_tile[groupId + 8] + g_tile[groupId + 9]);
        g_tile[groupId] = avgPixel;
        g_result2[g_threadId.xy >> 1] = avgPixel;
    }
    GroupMemoryBarrierWithGroupSync();

    if((parity & 3) == 0) {
         avgPixel = 0.25f * (avgPixel + g_tile[groupId + 2] + g_tile[groupId + 16] + g_tile[groupId + 18]);
        g_tile[groupId] = avgPixel;
        g_result3[g_threadId.xy >> 2] = avgPixel;
    }
    GroupMemoryBarrierWithGroupSync();

    if((parity & 7) == 0) {
         avgPixel = 0.25f * (avgPixel + g_tile[groupId + 4] + g_tile[groupId + 32] + g_tile[groupId + 36]);
        g_result4[g_threadId.xy >> 3] = avgPixel;
    }
}

