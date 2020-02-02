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
	float3 flatLightDir : FLD;
};
void vMain(in uint id : SV_VertexID, out V2P o) {
	float3 position = positions[id];
	o.position = mul(mvp, float4(position, 1));
	o.normal = position;
	o.flatLightDir = normalize(float3(lightDir.x, 0, lightDir.z));
}
Texture2D worldTex : register(t0);
SamplerState samplerState : register(s0);
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 world = worldTex.SampleLevel(samplerState, i.position.xy * invScreenSize, 0);
	float3 normal = normalize(i.normal);
	float earthBloom = earthBloomMult * 0.5f * (1 - saturate(dot(normal, float3(0, 1, 0))));
	float sunBloom = sunBloomMult * pow(saturate(2 / PI * asin(map(dot(normalize(normal * float3(sunBloomMult,1, sunBloomMult)), i.flatLightDir), -1, 1, 0, 1))), 2);
	oColor = float4(lerp(world.xyz, lerp(skyColor, sunColor, sunBloom + earthBloom), world.w * world.w), 1);
}