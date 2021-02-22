#pragma once

#include "Utils/Math/MathConstants.slangh"
import Scene.Shading;
import Scene.Raster;

float4x4 look_at_rh(float3 eye, float3 center, float3 up) {
    float3 f = normalize(center - eye);
    float3 s = normalize(cross(f, up));
    float3 u = cross(s, f);

    float4x4 result = {};
    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] =-f.x;
    result[1][2] =-f.y;
    result[2][2] =-f.z;
    result[3][0] =-dot(s, eye);
    result[3][1] =-dot(u, eye);
    result[3][2] = dot(f, eye);
    result[3][3] = 1.0f;
    return result;
}

float4x4 orth_rh_zo(float left, float right, float bottom, float top, float near, float far) {
    float4x4 result = {};
    result[0][0] = 2.0f / (right - left);
    result[1][1] = 2.0f / (top - bottom);
#ifdef FALCOR_VK
    result[1][1] = -result[1][1];
#endif
    result[2][2] = - 1.0f / (far - near);
    result[3][0] = - (right + left) / (right - left);
    result[3][1] = - (top + bottom) / (top - bottom);
    result[3][2] = - near / (far - near);
    result[3][3] = 1.0f;
    return result;
}

// faclor is right handed
float4x4 calculate_orth_view_proj(AABB bound, int32_t dim) {
    float4x4 result = {};
    float3 center = bound.center();
    float radius = bound.radius(); 

    switch(dim) {
        case 0:
        default: {
            // yz space
            float4x4 yzView = look_at_rh(float3(center.x + radius * 1.5f, center.y, center.z), center, float3(0, 1, 0));
            float4x4 yzProj = orth_rh_zo(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
            result = mul(yzView, yzProj);
        }
        break;

        case 1: {
            // xz space;
            float4x4 xzView = look_at_rh(float3(center.x, center.y + radius * 1.5f, center.z), center, float3(1, 0, 0));
            float4x4 xzProj = orth_rh_zo(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
            result = mul(xzView, xzProj);
        }
        break;

        case 2: {
            // xy space
            float4x4 xyView = look_at_rh(float3(center.x, center.y, center.z + radius * 1.5f), center, float3(0, 1, 0));
            float4x4 xyProj = orth_rh_zo(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
            result = mul(xyView, xyProj);
        }
        break;
    }

    return result;

}

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
cbuffer CB {
    voxel_meta gVoxelMeta;
}


[maxvertexcount(3)]
void gs_main(triangle VSOut input[3], uint InstanceID : SV_GSInstanceID, inout TriangleStream<VSOut> outStream) {

    VSOut outputData = (VSOut)0;

    float3 posW0 = input[0].posW;
    float3 posW1 = input[1].posW;
    float3 posW2 = input[2].posW;

    // cacluate AABB
    AABB bound = AABB::create(gVoxelMeta.Min, gVoxelMeta.Max);

    float3 edgeW01 = posW1 - posW0;
    float3 edgeW02 = posW2 - posW0;

    // decide the projection area in each dim
    float areaXY = abs(edgeW01.x * edgeW02.y - edgeW02.x * edgeW01.y) / 2.0f;
    float areaXZ = abs(edgeW01.x * edgeW02.z - edgeW02.x * edgeW01.z) / 2.0f;
    float areaYZ = abs(edgeW01.y * edgeW02.z - edgeW02.y * edgeW01.z) / 2.0f;

    // choose projection matrix
    int32_t dim = 0;
    if(areaYZ > areaXY && areaYZ > areaXZ) dim = 0;
    else dim = areaXZ > areaXY ? 1 : 2;

    float4x4 viewProjMat = calculate_orth_view_proj(bound, dim);

    // reprojection & output
    outputData = input[0];
    outputData.posH = mul(float4(posW0, 1.0f), viewProjMat);
    outStream.Append(outputData);

    outputData = input[1];
    outputData.posH = mul(float4(posW1, 1.0f), viewProjMat);
    outStream.Append(outputData);

    outputData = input[2];
    outputData.posH = mul(float4(posW2, 1.0f), viewProjMat);
    outStream.Append(outputData);

    outStream.RestartStrip();
}

import Scene.TextureSampler;
import Scene.Material.MaterialData;
import Utils.Math.MathHelpers;
#include "Scene/Material/MaterialDefines.slangh"

RWTexture3D<uint32_t> gPackedAlbedo;
RWTexture3D<uint32_t> gPackedNormal;

float3 get_normal_map<L:ITextureSampler>(MaterialData md, MaterialResources mr, float2 uv, float3 normalW, float4 tangentW, L lod)
{
    uint mapType = EXTRACT_NORMAL_MAP_TYPE(md.flags);
    if (mapType == NormalMapUnused) return normalW;

    float3 mapN = lod.sampleTexture(mr.normalMap, mr.samplerState, uv).xyz;
    switch(mapType)
    {
    case NormalMapRGB:
        mapN = RgbToNormal(mapN);
        break;
    case NormalMapRG:
        mapN = RgToNormal(mapN.rg);
        break;
    default:
        return normalW;
    }

    float NdotT = dot(tangentW.xyz, normalW);
    bool nonParallel = abs(NdotT) < 0.9999f;
    bool nonZero = dot(tangentW.xyz, tangentW.xyz) > 0.f;

    float3 T,B;

    bool valid = tangentW.w != 0.f && nonZero && nonParallel;
    if (valid)
    {
        T = normalize(tangentW.xyz - normalW * NdotT);
        B = cross(normalW, T) * tangentW.w;
    }
    else
    {
        T = perp_stark(normalW);
        B = cross(normalW, T);
    }

    return normalize(T * mapN.x + B * mapN.y + normalW * mapN.z);
}


void image_atomic_avg_RGBA8(RWTexture3D<uint32_t> img, uint3 coords, float4 newVal) {

    newVal.xyz *= 255.0f;
    uint32_t packedNewVal = RGBA8_255_to_uint32(newVal);
    uint32_t lastPackedVal = 0;
    uint32_t curPackedVal = 0;
    float4 curVal = {};
    uint32_t numIterations = 0;

    [allow_uav_condition] while(true) {

        InterlockedCompareExchange(img[coords], lastPackedVal, packedNewVal, curPackedVal);
        if(curPackedVal == lastPackedVal || numIterations >= g_maxAvgIterations) break;

        lastPackedVal = curPackedVal;
        curVal = uint32_to_RGBA8_255(curPackedVal);
        curVal.xyz *= curVal.a;
        curVal += newVal;
        curVal.xyz /= curVal.a;
        packedNewVal = RGBA8_255_to_uint32(curVal);
        ++numIterations;
    }

    newVal = uint32_to_RGBA8_255(packedNewVal);
    newVal.a = 255.0;
    packedNewVal = RGBA8_255_to_uint32(newVal);
    img[coords] = packedNewVal;
}


float4 ps_main(VSOut vOut) : SV_TARGET {

    float3 posW = vOut.posW;
    uint3 cellIndex = floor((posW - gVoxelMeta.Min) / gVoxelMeta.CellSize);

    if(cellIndex.x >= 0 && cellIndex.x < gVoxelMeta.CellDim.x
        && cellIndex.y >= 0 && cellIndex.y < gVoxelMeta.CellDim.y
        && cellIndex.z >= 0 && cellIndex.z < gVoxelMeta.CellDim.z) {

        ImplicitLodTextureSampler lod = { };
        MaterialData md = gScene.materials[vOut.materialID];
        MaterialResources mr = gScene.materialResources[vOut.materialID];

        float4 baseColor = sampleTexture(mr.baseColor, mr.samplerState, vOut.texC, md.baseColor, EXTRACT_DIFFUSE_TYPE(md.flags), lod);
        baseColor.a = 1.0f;

        float4 normalW = {};
        normalW.xyz = get_normal_map(md, mr, vOut.texC, vOut.normalW, vOut.tangentW, lod);
        normalW.xyz = normalW.xyz * 0.5f + 0.5f;
        normalW.w = 1.0f;

        image_atomic_avg_RGBA8(gPackedAlbedo, cellIndex, baseColor);
        image_atomic_avg_RGBA8(gPackedNormal, cellIndex, normalW);
    }

    return float4(0,0,0,0);
}
