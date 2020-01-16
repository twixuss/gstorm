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
float4 getPosition(uint data) {
	float4 position = float4(positions[(data >> 3) & 0x7], 1);
	position.x += (data >> 16) & 0x1F;
	position.y += (data >> 11) & 0x1F;
	position.z += (data >> 6 ) & 0x1F;
	return position;
}
float3 getNormal(uint data) {
	return normals[data & 0x7];
}
StructuredBuffer<Vertex> vertices : register(t0);
#define MAP(type)                                       \
type map(type v, type si, type sa, type di, type da) {  \
	return (v - si) / (sa - si) * (da - di) + di;       \
}

MAP(float)
MAP(float2)
MAP(float3)
MAP(float4)

void vMain(in uint id : SV_VertexID, out V2P o) {
	Vertex v = vertices[id];
	float4 position = getPosition(v.data);
	float3 normal = getNormal(v.data);
	o.position = mul(mvp, position);
	o.uv = v.uv;
	o.color = map(dot(normal, normalize(float3(3, 5, 1))), -1, 1, 0.25, 1);
	float3 wpos = mul(model, position).xyz;
	float dist = distance(wpos, camPos);
	o.fog = pow(min(dist / fogDistance, 1), 3);
	//o.fog = (1 - exp(-dist / fogDistance * 2));
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	oColor = solidColor;
}