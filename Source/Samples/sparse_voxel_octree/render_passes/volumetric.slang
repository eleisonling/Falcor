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

[maxvertexcount(3)]
void gs_main(triangle VSOut input[3], uint InstanceID : SV_GSInstanceID, inout TriangleStream<VSOut> outStream) {

    VSOut outputData = (VSOut)0;

    float3 posW0 = input[0].posW;
    float3 posW1 = input[1].posW;
    float3 posW2 = input[2].posW;

    // cacluate AABB
    AABB bound = AABB::create(FLT_MAX, -FLT_MAX);
    bound.include(posW0);
    bound.include(posW1);
    bound.include(posW2);

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

#include "Samples/sparse_voxel_octree/render_passes/voxel_meta.slangh"
import Scene.TextureSampler;
import Scene.Material.MaterialData;
#include "Scene/Material/MaterialDefines.slangh"

cbuffer CB {
    voxel_meta gVoxelMeta;
}
RWByteAddressBuffer gVoxelColor;

float4 ps_main(VSOut vOut) : SV_TARGET {

    float3 posW = vOut.posW;
    uint3 cellIndex = floor((posW - gVoxelMeta.Min) / gVoxelMeta.CellSize);

    if(cellIndex.x >= 0 && cellIndex.x < gVoxelMeta.CellDim.x
        && cellIndex.y >= 0 && cellIndex.y < gVoxelMeta.CellDim.y
        && cellIndex.z >= 0 && cellIndex.z < gVoxelMeta.CellDim.z) {

        // get linear index
        uint32_t linearIndex = cellIndex.z * gVoxelMeta.CellDim.y * gVoxelMeta.CellDim.x + cellIndex.y * gVoxelMeta.CellDim.x + cellIndex.x;
        // jump to first address
        uint32_t address = linearIndex * 4;

        ImplicitLodTextureSampler lod = { };
        MaterialData md = gScene.materials[vOut.materialID];
        MaterialResources mr = gScene.materialResources[vOut.materialID];
        float4 baseColor = sampleTexture(mr.baseColor, mr.samplerState, vOut.texC, md.baseColor, EXTRACT_DIFFUSE_TYPE(md.flags), lod);
        uint32_t compressedColor = encode_translucent_RGBA_uint(baseColor);
        uint32_t originColor = 0;

        // atomic float max
        gVoxelColor.InterlockedMax(address, compressedColor, originColor);
    }

    return float4(0,0,0,0);
}
