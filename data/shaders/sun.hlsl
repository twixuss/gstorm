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
float getMask(float NLlin, float fov, float b) {
	return pow(saturate(NLlin * 180 / (180 - fov)), 180 / (fov * b));
}
void pMain(in V2P i, out float4 oColor : SV_Target, out float oDepth : SV_Depth) {
	oDepth = 0.9999999f;
	float3 normal = normalize(i.normal);
	float NL = dot(normal, lightDir);
	float NLlin = 2 / PI * asin(NL);
	float sun  = getMask( NLlin, 1, 5);
	float moon = getMask(-NLlin, 1, 2);
	oColor = float4(lerp(0.8, sunColor, saturate(map(NL,-1,1,-2,2))), 1 - sun - moon);
}