#include "chunkVertex.h"
#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 bitangent : BITANGENT;
	float3 wpos : WPOS;
	float2x2 nrmRot : NRMROT;
	float2 nrmFlip : NRMFLIP;
	float2 uv : UV;
	float fog : FOG;
};
StructuredBuffer<ChunkVertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);
Texture2D albedoTex : register(t2);
Texture2D normalTex : register(t3);
void vMain(in uint id : SV_VertexID, out V2P o) {
	ChunkVertex v = vertices[indices[id]];

	float4 position = getPosition(v);
	o.position = mul(mvp, position);
	//o.position = mul(modelCamPos, position);
	//o.position.y += cos(length(o.position.xyz)) * length(o.position.xz) * .02;
	//o.position = mul(camRotProjection, o.position);
	o.wpos = mul(model, position).xyz;

	getNormal(v, o.normal, o.tangent, o.bitangent);
	o.uv = getUv(v, o.nrmRot, o.nrmFlip);

	float dist = distance(o.wpos, camPos);
	o.fog = getFog(dist);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float3 L = lightDir;
	float3 V = normalize(i.wpos - camPos);
	float3 H = normalize(L - V);
	float4 color = albedoTex.Sample(pointSamplerState, i.uv);
	clip(color.a - 0.5f);
	float3 normalMap = normalTex.Sample(pointSamplerState, i.uv).xyz * 2 - 1;
	normalMap.xy = mul(i.nrmRot, normalMap.xy) * i.nrmFlip;
	float3 N = calcNormal(i.normal, i.tangent, i.bitangent, normalize(normalMap));
	float3 diffuse = color.xyz * (
		//(sunColor * saturate(map(dot(N, L), -1, 1, 0, 1))) +
		(sunColor * .75 * saturate(map(dot(N, L), -1, 1, 0, 1))) +
		(skyColor * 1.5));

	float3 specular = 0;
	specular = sunColor * pow(sdot(N, H), 15) * .5 * pow3i(sdot(N, L));

	oColor = float4(diffuse + specular, i.fog * solidColor.w);
	//oColor = float4(albedoTex.Sample(pointSamplerState, frac(i.wpos.xz - .5) / 32).xyz, 0);
}