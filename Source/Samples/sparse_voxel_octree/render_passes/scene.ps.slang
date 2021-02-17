#pragma once

import Scene.Shading;
import Scene.Raster;

cbuffer PerFrameCB : register(b0)
{
    bool gConstColor;
};

float4 main(VSOut vOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET
{
    if(gConstColor)
    {
        return float4(0, 1, 0, 1);
    }
    else
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
}
