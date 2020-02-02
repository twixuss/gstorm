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
float getMask(float NLlin, float s, float b) {
	return saturate((NLlin * 1 / (1 - s) - 1 / (1 - b)) * 1 / b + 1 / (1 - b));
}
float3 getColor(float3 base, float l, float s) {
	return base * (1 / l - 1) * s / (1 - s);
}
void pMain(in V2P i, out float4 oColor : SV_Target, out float oDepth : SV_Depth) {
	oDepth = 0.9999999f;
	float3 normal = normalize(i.normal);
	float NL = dot(normal, lightDir);
	float NLlin = 2 / PI * asin(NL);
	float sunSize = 5;
	float s = 1.f / 360.f * sunSize;
	//float moon = getMask(-NLlin, 1, 1);
	//oColor = float4(lerp(0.8, sunColor, saturate(map(NL,-1,1,-2,2))), 1 - sun - moon);
	oColor = float4(lerp(0.8,
						 sunColor,
						 saturate(map(NL, -1, 1, -2, 2))),
					1 - (getMask(NLlin, s, .001) + getMask(-NLlin, s, .001)));
}