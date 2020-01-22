#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float2 uv : UV;
};
struct Vertex {
	float3 position;
	float2 uv;
};
StructuredBuffer<Vertex> vertices : register(t0);
Texture2D albedoTex : register(t2);
SamplerState samplerState : register(s0);
void vMain(in uint id : SV_VertexID, out V2P o) {
	Vertex v = vertices[id];
	o.position = mul(mvp, float4(v.position, 1));
	o.uv = v.uv;
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 color = albedoTex.Sample(samplerState, i.uv);
	clip(color.a - .5);
	color.a = 0;
	oColor = color;
}