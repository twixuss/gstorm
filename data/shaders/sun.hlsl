#include "common.h"
static const float3 positions[] = {
	float3(-1,-1, 1),
	float3(-1, 1, 1),
	float3( 1, 1, 1),
	float3(-1,-1, 1),
	float3( 1, 1, 1),
	float3( 1,-1, 1),

	float3(-1,-1,-1),
	float3( 1, 1,-1),
	float3(-1, 1,-1),
	float3(-1,-1,-1),
	float3( 1,-1,-1),
	float3( 1, 1,-1),
	
	float3(-1,-1,-1),
	float3(-1,-1, 1),
	float3( 1,-1, 1),
	float3(-1,-1,-1),
	float3( 1,-1, 1),
	float3( 1,-1,-1),

	float3(-1, 1,-1),
	float3( 1, 1, 1),
	float3(-1, 1, 1),
	float3(-1, 1,-1),
	float3( 1, 1,-1),
	float3( 1, 1, 1),
	
	float3(-1,-1,-1),
	float3(-1, 1, 1),
	float3(-1,-1, 1),
	float3(-1,-1,-1),
	float3(-1, 1,-1),
	float3(-1, 1, 1),

	float3( 1,-1,-1),
	float3( 1,-1, 1),
	float3( 1, 1, 1),
	float3( 1,-1,-1),
	float3( 1, 1, 1),
	float3( 1, 1,-1),
};
struct V2P {
	float4 position : SV_Position;
	float3 normal : NORMAL;
};
void vMain(in uint id : SV_VertexID, out V2P o) {
	float3 position = positions[id];
	o.position = mul(mvp, float4(position, 1));
	o.normal = position;
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float3 normal = normalize(i.normal);
	float NL = dot(normal, lightDir);
	float sun = saturate((NL - 0.999) * 5000);
	float moon = saturate((-NL - 0.999) * 5000);
	oColor = float4(lerp(0.8, sunColor, saturate(map(NL,-1,1,-2,2))), sun + moon);
}