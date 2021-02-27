#pragma once
import Scene.Raster;

cbuffer CB {
    float4x4 g_light_view_proj;
}

struct vs_out {
    float4 posH                     : SV_POSITION;
};

vs_out vs_main(VSIn vIn) {
    vs_out out = {};
    float4x4 worldMat = gScene.getWorldMatrix(vIn.meshInstanceID);
    float4 posW = mul(float4(vIn.pos, 1.f), worldMat);
    out.posH = mul(posW, g_light_view_proj);

    return out;
}

void ps_main(vs_out vOut) {}
