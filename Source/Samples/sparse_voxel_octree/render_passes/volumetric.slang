#pragma once

#include "Utils/Math/MathConstants.slangh"
import Scene.Shading;
import Scene.Raster;


// faclor is right handed
float4x4 calculate_orth_view_proj(AABB bound, int32_t dim) {
   float4x4 view = (float4x4)0;
   float4x4 proj = (float4x4)0;

   switch(dim) {
   case 0:
   default:
   {
        
   }
   break;

   case 1:
   {
        
   }
   break;

   case 2:
   {
        
   }
   break;
   }

   return mul(proj * view);
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
    float areaYZ = abs(edgeW01.y * edgeW02.z - edgeW02.y * edgeW01.z) / 2.0f

    // choose projection matrix
    int32_t dim = 0;
    if(areaYZ > areaXY && areaYZ > areaXZ) dim = 0;
    else dim = areaXZ > areaXY ? 1 : 2;

    float4x4 viewProjMat = caculate_orth_view_proj(bound, dim);

    // reprojection & output
    outputData = input[0];
    outputData.posH = mul(float4(posW0, 1.0f), vewProjMat);
    outStream.Append(outputData);

    outputData = input[1];
    outputData.posH = mul(float4(posW1, 1.0f), viewProjMat);
    outStream.Append(outputData);

    outputData = input[2];
    outputData.posH = mul(float4(posW2, 1.0f), viewProjMat);
    outStream.Append(outputData);

    outStream.RestartStrip();
}

float4 ps_main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {
    float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir);
    float4 finalColor = float4(0, 0, 0, 1);

    for (int i = 0; i < gScene.getLightCount(); i++) {
        finalColor.rgb += evalMaterial(sd, gScene.getLight(i), 1).color;
    }

    return finalColor;
}
