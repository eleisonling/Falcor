#pragma once

import Scene.Shading;
import Scene.Raster;

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
