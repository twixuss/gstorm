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
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float3 normal = normalize(i.normal);
	float NL = dot(normal, lightDir);
	float sun = saturate((NL - 0.999) * 5000);
	float moon = saturate((-NL - 0.999) * 5000);
	float earthBloom = earthBloomMult * 0.5f * (1 - abs(dot(normal, float3(0, 1, 0))));
	float sunBloom = sunBloomMult * 0.5 * saturate(dot(normal, i.flatLightDir));
	oColor = float4(lerp(skyColor, sunColor, sunBloom + earthBloom) + sun * sunColor + moon * 0.8, 1);
}