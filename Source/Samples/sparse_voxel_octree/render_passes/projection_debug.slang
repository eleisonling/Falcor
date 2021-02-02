#pragma once
import Scene.Shading;
import Scene.Raster;

VSOut vs_main(VSIn vIn)
{
    VSOut vOut{};
    float4x4 worldMat = gScene.getWorldMatrix(vIn.meshInstanceID);
    float4 posW = mul(float4(vIn.pos, 1.f), worldMat);
    vOut.posW = posW.xyz;
    vOut.posH = mul(posW, gScene.camera.getViewProj());

    vOut.meshInstanceID = vIn.meshInstanceID;
    vOut.materialID = gScene.getMaterialID(vIn.meshInstanceID);

    vOut.texC = vIn.texC;
    vOut.normalW = mul(vIn.unpack().normal, gScene.getInverseTransposeWorldMatrix(vIn.meshInstanceID));
    float4 tangent = vIn.unpack().tangent;
    vOut.tangentW = float4(mul(tangent.xyz, (float3x3)gScene.getWorldMatrix(vIn.meshInstanceID)), tangent.w);
    
    return vOut;
}

float4 ps_main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET
{    
    float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir);
    float4 finalColor = float4(0, 0, 0, 1);

    for (int i = 0; i < gScene.getLightCount(); i++)
    {
        finalColor.rgb += evalMaterial(sd, gScene.getLight(i), 1).color;
    }
    return finalColor;
}
