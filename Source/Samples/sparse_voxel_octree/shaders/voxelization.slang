#pragma once

#include "Utils/Math/MathConstants.slangh"
#include "Samples/sparse_voxel_octree/shaders/voxelization_meta.slangh"
import Scene.Shading;
import Scene.Raster;

cbuffer CB {
    voxelization_meta bufVoxelMeta;
    StructuredBuffer<float4x4> matViewProjections;
}

struct GSOut{
    VSOut vsData;
    uint triangleIndex : SV_PrimitiveID;
};


[maxvertexcount(3)]
void gs_main(triangle VSOut input[3], uint InstanceID : SV_GSInstanceID, uint triangleIndex : SV_PrimitiveID, inout TriangleStream<GSOut> outStream) {

    GSOut outputData = (GSOut)0;

    float3 posW0 = input[0].posW;
    float3 posW1 = input[1].posW;
    float3 posW2 = input[2].posW;

    // cacluate AABB
    AABB bound = AABB::create(bufVoxelMeta.Min, bufVoxelMeta.Max);

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

    float4x4 viewProjMat = matViewProjections[dim];

    // reprojection & output
    outputData.vsData = input[0];
    outputData.vsData.posH = mul(float4(posW0, 1.0f), viewProjMat);
    outputData.triangleIndex = triangleIndex;
    outStream.Append(outputData);

    outputData.vsData = input[1];
    outputData.vsData.posH = mul(float4(posW1, 1.0f), viewProjMat);
    outputData.triangleIndex = triangleIndex;
    outStream.Append(outputData);

    outputData.vsData = input[2];
    outputData.vsData.posH = mul(float4(posW2, 1.0f), viewProjMat);
    outputData.triangleIndex = triangleIndex;
    outStream.Append(outputData);

    outStream.RestartStrip();
}

import Scene.TextureSampler;
import Scene.Material.MaterialData;
import Utils.Math.MathHelpers;
#include "Scene/Material/MaterialDefines.slangh"

RWTexture3D<uint32_t> texPackedAlbedo;
RWTexture3D<uint32_t> texPackedNormal;

void image_atomic_avg_RGBA8(RWTexture3D<uint32_t> img, uint3 coords, float4 newVal) {

    newVal.xyz *= PACKED_RGB_FACTOR;
    uint32_t packedNewVal = RGBA8_255_to_uint32(newVal);
    uint32_t lastPackedVal = 0;
    uint32_t curPackedVal = 0;
    float4 curVal = {};
    uint32_t numIterations = 0;

    [allow_uav_condition] while(true) {

        InterlockedCompareExchange(img[coords], lastPackedVal, packedNewVal, curPackedVal);
        if(curPackedVal == lastPackedVal || numIterations >= MAX_AVG_ITERATIONS) break;

        lastPackedVal = curPackedVal;
        curVal = uint32_to_RGBA8_255(curPackedVal);
        curVal.xyz *= curVal.a;
        curVal += newVal;
        curVal.xyz /= curVal.a;
        packedNewVal = RGBA8_255_to_uint32(curVal);
        ++numIterations;
    }
}

float4 ps_main(GSOut gOut) : SV_TARGET {

    float3 posW = gOut.vsData.posW;
    uint3 cellIndex = floor((posW - bufVoxelMeta.Min) / bufVoxelMeta.CellSize);

    if(cellIndex.x >= 0 && cellIndex.x < bufVoxelMeta.CellDim.x
        && cellIndex.y >= 0 && cellIndex.y < bufVoxelMeta.CellDim.y
        && cellIndex.z >= 0 && cellIndex.z < bufVoxelMeta.CellDim.z) {

        float3 viewDir = normalize(gScene.camera.getPosition() - gOut.vsData.posW);
        ShadingData sd = prepareShadingData(gOut.vsData, gOut.triangleIndex, viewDir);

        float4 baseColor = float4(sd.diffuse, 1.0f);
        float4 normalW = float4(sd.N, 1.0f);

        image_atomic_avg_RGBA8(texPackedAlbedo, cellIndex, baseColor);
        image_atomic_avg_RGBA8(texPackedNormal, cellIndex, normalW);
    }

    return float4(0,0,0,0);
}
