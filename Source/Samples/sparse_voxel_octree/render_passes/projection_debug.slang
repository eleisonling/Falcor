#pragma once
import Scene.Shading;
import Scene.Raster;
import Utils.Math.AABB;
#include "Samples/sparse_voxel_octree/render_passes/projection_debug.slangh"

cbuffer CB {
    projection_meta gProjectionMeta;
}

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
            float4x4 xyView = look_at_rh(float3(center.x, center.y, center.z + radius * 1.5f), center, float3(0, 1, 0));
            float4x4 xyProj = orth_rh_zo(-radius, radius, -radius, radius, 0.1f, 4.0f * radius);
            result = mul(xyView, xyProj);
        }
        break;
    }

        return result;
}

VSOut vs_main(VSIn vIn) {
    VSOut vOut = (VSOut)0;
    float4x4 worldMat = gScene.getWorldMatrix(vIn.meshInstanceID);
    float4 posW = mul(float4(vIn.pos, 1.f), worldMat);
    vOut.posW = posW.xyz;

    AABB bound = AABB::create(gProjectionMeta.Min, gProjectionMeta.Max);
    float4x4 debugViewProj = calculate_orth_view_proj(bound, gProjectionMeta.Dim);
    vOut.posH = mul(posW, debugViewProj);

    vOut.meshInstanceID = vIn.meshInstanceID;
    vOut.materialID = gScene.getMaterialID(vIn.meshInstanceID);

    vOut.texC = vIn.texC;
    vOut.normalW = mul(vIn.unpack().normal, gScene.getInverseTransposeWorldMatrix(vIn.meshInstanceID));
    float4 tangent = vIn.unpack().tangent;
    vOut.tangentW = float4(mul(tangent.xyz, (float3x3)gScene.getWorldMatrix(vIn.meshInstanceID)), tangent.w);
    
    return vOut;
}

float4 ps_main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {    
    float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir);
    float4 finalColor = float4(0, 0, 0, 1);

    for (int i = 0; i < gScene.getLightCount(); i++)
    {
        finalColor.rgb += evalMaterial(sd, gScene.getLight(i), 1).color;
    }
    return finalColor;
}
