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


Texture2D<float3> g_blurInput;
RWTexture2D<float3> g_blurResult;


groupshared uint32_t g_cacheR[128];
groupshared uint32_t g_cacheG[128];
groupshared uint32_t g_cacheB[128];


static const float g_weights[5] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float3 blur_pixels( float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i )
{
    return g_weights[0]*e + g_weights[1]*(d+f) + g_weights[2]*(c+g) + g_weights[3]*(b+h) + g_weights[4]*(a+i);
}

void store_2_pixels(uint32_t index, float3 pixel1, float3 pixel2) {
    g_cacheR[index] = f32tof16(pixel1.r) | f32tof16(pixel2.r) << 16;
    g_cacheG[index] = f32tof16(pixel1.g) | f32tof16(pixel2.g) << 16;
    g_cacheB[index] = f32tof16(pixel1.b) | f32tof16(pixel2.b) << 16;
}

void load_2_pixels( uint32_t index, out float3 pixel1, out float3 pixel2 )
{
    uint32_t rr = g_cacheR[index];
    uint32_t gg = g_cacheG[index];
    uint32_t bb = g_cacheB[index];
    pixel1 = float3( f16tof32(rr      ), f16tof32(gg      ), f16tof32(bb      ) );
    pixel2 = float3( f16tof32(rr >> 16), f16tof32(gg >> 16), f16tof32(bb >> 16) );
}

void store_pixel( uint32_t index, float3 pixel )
{
    g_cacheR[index] = asuint(pixel.r);
    g_cacheG[index] = asuint(pixel.g);
    g_cacheB[index] = asuint(pixel.b);
}

void load_pixel( uint32_t index, out float3 pixel )
{
    pixel = asfloat( uint3(g_cacheR[index], g_cacheG[index], g_cacheB[index]) );
}


void blur_horizontally( uint32_t outIndex, uint32_t leftMostIndex )
{
    float3 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
    load_2_pixels( leftMostIndex + 0, s0, s1 );
    load_2_pixels( leftMostIndex + 1, s2, s3 );
    load_2_pixels( leftMostIndex + 2, s4, s5 );
    load_2_pixels( leftMostIndex + 3, s6, s7 );
    load_2_pixels( leftMostIndex + 4, s8, s9 );
    
    store_pixel(outIndex  , blur_pixels(s0, s1, s2, s3, s4, s5, s6, s7, s8));
    store_pixel(outIndex+1, blur_pixels(s1, s2, s3, s4, s5, s6, s7, s8, s9));
}

void blur_vertically( uint2 pixelCoord, uint32_t topMostIndex )
{
    float3 s0, s1, s2, s3, s4, s5, s6, s7, s8;
    load_pixel( topMostIndex   , s0 );
    load_pixel( topMostIndex+ 8, s1 );
    load_pixel( topMostIndex+16, s2 );
    load_pixel( topMostIndex+24, s3 );
    load_pixel( topMostIndex+32, s4 );
    load_pixel( topMostIndex+40, s5 );
    load_pixel( topMostIndex+48, s6 );
    load_pixel( topMostIndex+56, s7 );
    load_pixel( topMostIndex+64, s8 );

    g_blurResult[pixelCoord] = blur_pixels(s0, s1, s2, s3, s4, s5, s6, s7, s8);
}

[numthreads(8, 8, 1)]
void blur(uint3 Gid : SV_GroupID, uint32_t3 GTid : SV_GroupThreadID, uint32_t3 DTid : SV_DispatchThreadID){
    int2 groupUL = (Gid.xy << 3) - 4;
    int2 threadUL = (GTid.xy << 1) + groupUL;

    int32_t destIdx = GTid.x + (GTid.y << 4);
    store_2_pixels(destIdx + 0, g_blurInput[threadUL + uint2(0, 0)], g_blurInput[threadUL + uint2(1, 0)]);
    store_2_pixels(destIdx + 8, g_blurInput[threadUL + uint2(2, 1)], g_blurInput[threadUL + uint2(1, 1)]);
    GroupMemoryBarrierWithGroupSync();

    uint32_t row = GTid.y << 4;
    blur_horizontally(row + (GTid.x << 1), row + GTid.x + (GTid.x & 4));
    GroupMemoryBarrierWithGroupSync();

    blur_vertically(DTid.xy, (GTid.y << 3) + GTid.x);
}
