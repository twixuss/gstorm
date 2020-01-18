#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float3 color : COLOR;
	float2 uv : UV;
	float fog : FOG;
};
struct Vertex {
	float3 position;
	float3 normal;
	float2 uv;
};
StructuredBuffer<Vertex> vertices : register(t0);
Texture2D grassTex : register(t2);
SamplerState samplerState : register(s0);

void vMain(in uint id : SV_VertexID, out V2P o) {
	Vertex v = vertices[id];
	o.position = mul(mvp, float4(v.position, 1));
	o.uv = v.uv;
	o.color = map(dot(v.normal, normalize(float3(3, 5, 1))), -1, 1, 0.25, 1);
	float3 wpos = mul(model, float4(v.position, 1)).xyz;
	float dist = distance(wpos, camPos);
	o.fog = getFog(dist);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 color = grassTex.Sample(samplerState, i.uv);
	clip(color.a - 0.5f);
	oColor = lerp(color * float4(i.color, 1) * solidColor, fogColor, i.fog);
}