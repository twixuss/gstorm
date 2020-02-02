#ifndef _COMMON_H_
#define _COMMON_H_
#define PI 3.141592653f
#define PI2 (PI * 2.0f)
#define INVPI (1.0f / PI)
#define INVPI2 (1.0f / PI2)
#define DEG2RAD(deg) ((deg) / 360.0f * PI2)
#define RAD2DEG(rad) ((rad) / PI2 * 360.0f)
#define ROOT2 1.414213538f
#define ROOT3 1.732050776f
cbuffer draw : register(b0) {
	matrix mvp;
	matrix model;
	matrix modelCamPos;
	matrix camRotProjection;
	float4 solidColor;
}
cbuffer frame : register(b1) {
	float3 camPos;
	float time;

	float3 lightDir;
	float dayTime;

	float3 skyColor;
	float earthBloomMult;

	float3 sunColor;
	float sunBloomMult;
}
cbuffer scene : register(b2) {
	float fogDistance;
}
cbuffer screen : register(b3) {
	matrix projection;
	float2 sampleOffset;
	float2 invScreenSize;
}
float getFog(float dist) {
	return min(dist / fogDistance, 1);
}
#define DECLOV DECLFN(float) DECLFN(float2) DECLFN(float3) DECLFN(float4)
#define DECLFN(type) type map(type v,type sn,type sx,type dn,type dx){return(v-sn)/(sx-sn)*(dx-dn)+dn;}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow2(type v){return v*v;}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow3(type v){return v*v*v;}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow4(type v){return v*v*v*v;}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow2i(type v){return 1-pow2(1-v);}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow3i(type v){return 1-pow3(1-v);}
DECLOV
#undef DECLFN
#define DECLFN(type) type pow4i(type v){return 1-pow4(1-v);}
DECLOV
#undef DECLFN
#undef DECLOV

float3 calcNormal(float3 normal, float3 tangent, float3 bitangent, float3 tangentSpaceNormal) {
	return normalize(
		tangentSpaceNormal.x * tangent +
		tangentSpaceNormal.y * bitangent +
		tangentSpaceNormal.z * normal
	);
}
float3 calcNormal(float3 normal, float3 tangent, float3 tangentSpaceNormal) {
	return calcNormal(normal, tangent, normalize(cross(normal, tangent)), tangentSpaceNormal);
}

float sdot(float2 a, float2 b) { return saturate(dot(a, b)); }
float sdot(float3 a, float3 b) { return saturate(dot(a, b)); }
float sdot(float4 a, float4 b) { return saturate(dot(a, b)); }

SamplerState pointSamplerState : register(s0);
SamplerState linearSamplerState : register(s1);

#endif