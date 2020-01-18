#ifndef _COMMON_H_
#define _COMMON_H_
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
float getFog(float dist) {
	return pow(min(dist / fogDistance, 1), 3);
}
#define MAP(type)                                       \
type map(type v, type si, type sa, type di, type da) {  \
	return (v - si) / (sa - si) * (da - di) + di;       \
}

MAP(float)
MAP(float2)
MAP(float3)
MAP(float4)
#endif