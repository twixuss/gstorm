#include "blockVertex.h"
cbuffer draw : register(b0) {
	matrix mvp;
	matrix model;
	float4 solidColor;
}
cbuffer frame : register(b1) {
	float3 camPos;
}
cbuffer scene : register(b2) {
	float4 fogColor;
	float fogDistance;
}
struct V2P {
	float4 position : SV_Position;
	float3 color : COLOR;
	float2 uv : UV;
	float fog : FOG;
};
static const float3 normals[6] = {
	float3( 1,0,0),
	float3(-1,0,0),
	float3(0, 1,0),
	float3(0,-1,0),
	float3(0,0, 1),
	float3(0,0,-1),
};
static const float3 positions[8] = {
	float3( 0.5, 0.5, 0.5),
	float3( 0.5, 0.5,-0.5),
	float3( 0.5,-0.5, 0.5),
	float3( 0.5,-0.5,-0.5),
	float3(-0.5, 0.5, 0.5),
	float3(-0.5, 0.5,-0.5),
	float3(-0.5,-0.5, 0.5),
	float3(-0.5,-0.5,-0.5),
};
float4 getPosition(BlockVertex v) {
	float4 position = float4(positions[(v.data0 >> 3) & 0x7], 1);
	position.x += (v.data0 >> 16) & 0x1F;
	position.y += (v.data0 >> 11) & 0x1F;
	position.z += (v.data0 >> 6 ) & 0x1F;
	return position;
}
float3 getNormal(BlockVertex v) {
	return normals[v.data0 & 0x7];
}
float2 getUv(BlockVertex v) {
	float2 uv;
	uv.x = ((v.data1 >> 6) & 0x3F) * ATLAS_ENTRY_SIZE;
	uv.y = (v.data1 & 0x3F) * ATLAS_ENTRY_SIZE;
	return uv;
}
StructuredBuffer<BlockVertex> vertices : register(t0);
Texture2D grassTex : register(t2);
SamplerState samplerState : register(s0);
#define MAP(type)                                       \
type map(type v, type si, type sa, type di, type da) {  \
	return (v - si) / (sa - si) * (da - di) + di;       \
}

MAP(float)
MAP(float2)
MAP(float3)
MAP(float4)

void vMain(in uint id : SV_VertexID, out V2P o) {
	BlockVertex v = vertices[id];
	float4 position = getPosition(v);
	float3 normal = getNormal(v);
	float2 uv = getUv(v);
	o.position = mul(mvp, position);
	o.uv = uv;
	o.color = map(dot(normal, normalize(float3(3, 5, 1))), -1, 1, 0.25, 1);
	float3 wpos = mul(model, position).xyz;
	float dist = distance(wpos, camPos);
	o.fog = pow(min(dist / fogDistance, 1), 3);
	//o.fog = (1 - exp(-dist / fogDistance * 2));
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	//oColor = grassTex.Sample(samplerState, i.uv) * i.color;
	float4 color = grassTex.Sample(samplerState, i.uv);
	clip(color.a - 0.5f);
	oColor = lerp(color * float4(i.color, 1) * solidColor, fogColor, i.fog);
	//oColor = float4(i.uv, 0, 1) * i.color;
}