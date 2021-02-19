#pragma once

Texture2D<uint32_t> g_lumaBuf;
RWByteAddressBuffer g_histogram;

groupshared uint32_t g_tileHistogram[256];

[numthreads(16, 16, 1)]
void gen_histogram(uint32_t GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID) {
    g_tileHistogram[GI] = 0;
    GroupMemoryBarrierWithGroupSync();
    uint32_t quantizedLogLuma = g_lumaBuf[DTid.xy];
    InterlockedAdd(g_tileHistogram[quantizedLogLuma], 1);
    GroupMemoryBarrierWithGroupSync();
    g_histogram.InterlockedAdd(GI * 4, g_tileHistogram[GI]);
}


#include "Samples/sparse_voxel_octree/render_passes/post_effects.slangh"
RWStructuredBuffer<exposure_meta> g_exposure;

cbuffer CB {
    float TargetLuminance;
    float AdaptationRate;
    float MinExposure;
    float MaxExposure;
    uint32_t PixelCount;
}

groupshared float gs_accum[256];

[numthreads(256, 1, 1)]
void adapt_exposure(uint32_t GI : SV_GroupIndex) {
    float weightedSum = (float)GI * (float)g_histogram.Load(GI * 4);

    for(uint32_t i = 1; i < 256; i *= 2) {
        gs_accum[GI] = weightedSum;
        GroupMemoryBarrierWithGroupSync();
        weightedSum += gs_accum[(GI + i) % 256];
        GroupMemoryBarrierWithGroupSync();
    }

    float minLog = g_exposure[0].MinLog;
    float maxLog = g_exposure[0].MaxLog;
    float logRange = g_exposure[0].DeltaLog;
    float rcpLogRange = g_exposure[0].InvDeltaLog;

    float weightedHistAvg = weightedSum / (max(1, PixelCount - g_histogram.Load(0))) - 1.0;
    float logAvgLuminance = exp2(weightedHistAvg / 254.0 * logRange + minLog);
    float targetExposure = TargetLuminance / logAvgLuminance;

    float exposure = g_exposure[0].Exposure;
    exposure = lerp(exposure, targetExposure, AdaptationRate);
    exposure = clamp(exposure, MinExposure, MaxExposure);

    if (GI == 0)
    {
        g_exposure[0].Exposure = exposure;
        g_exposure[0].InvExposure = 1.0 / exposure;
        float biasToCenter = (floor(weightedHistAvg) - 128.0) / 255.0;
        if (abs(biasToCenter) > 0.1)
        {
            minLog += biasToCenter * rcpLogRange;
            maxLog += biasToCenter * rcpLogRange;
        }

        logRange = maxLog - minLog;
        g_exposure[0].MinLog = minLog;
        g_exposure[0].MinLog = maxLog;
        g_exposure[0].DeltaLog = logRange;
        g_exposure[0].InvDeltaLog = 1.0 / logRange;
    }
}
