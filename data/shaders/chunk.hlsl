#include "chunkVertex.h"
#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 bitangent : BITANGENT;
	float2x2 nrmRot : NRMROT;
	float2 nrmFlip : NRMFLIP;
	float2 uv : UV;
	float fog : FOG;
};
StructuredBuffer<ChunkVertex> vertices : register(t0);
Texture2D albedoTex : register(t2);
Texture2D normalTex : register(t3);
SamplerState samplerState : register(s0);
void vMain(in uint id : SV_VertexID, out V2P o) {
	ChunkVertex v = vertices[id];

	float4 position = getPosition(v);
	o.position = mul(mvp, position);
	float3 wpos = mul(model, position).xyz;

	getNormal(v, o.normal, o.tangent, o.bitangent);
	o.uv = getUv(v, o.nrmRot, o.nrmFlip);

	float dist = distance(wpos, camPos);
	o.fog = getFog(dist);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 color = albedoTex.Sample(samplerState, i.uv);
	clip(color.a - 0.5f);
	float3 normalMap = normalTex.Sample(samplerState, i.uv).xyz * 2 - 1;
	normalMap.xy = mul(i.nrmRot, normalMap.xy) * i.nrmFlip;
	float3 normal = calcNormal(i.normal, i.tangent, i.bitangent, normalize(normalMap));
	//float3 normal = i.normal;
	float3 diffuse = color.xyz * (
		(sunColor * map(dot(normal, lightDir), -1, 1, 0, 1)) +
		(skyColor / PI * 2));
	oColor = float4(diffuse, i.fog * solidColor.w);
	//oColor.xyz = map(normal, -1, 1, 0, 1);
	//oColor.xyz = normalTex.Sample(samplerState, i.uv).xyz;
	//oColor.xyz = normal;
}