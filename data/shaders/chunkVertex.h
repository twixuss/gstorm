#ifndef _BLOCK_VERTEX_H_
#define _BLOCK_VERTEX_H_

#define ATLAS_SIZE 32
#define ATLAS_ENTRY_SIZE (1.0f/ATLAS_SIZE)

#define uvXOffset        11
#define uvYOffset        4
#define uvFlipOffset     2
#define uvRotationOffset 0

#define uvXMask        0x7F
#define uvYMask        0x7F
#define uvFlipMask     0x3
#define uvRotationMask 0x3

#ifdef __cplusplus
u32 makeVertexData0(int u, int v, int uf, int ur) {
	assert(u <= ATLAS_SIZE);
	assert(v <= ATLAS_SIZE);
	assert(uf < 4);
	assert(ur < 4);
	static_assert(CHUNK_WIDTH == 64);
	return (u << uvXOffset) | (v << uvYOffset) | (uf << uvFlipOffset) | (ur << uvRotationOffset);
}
#define DEFINIT {}
#else
#include "common.h"
#define u32 uint
#define V2 float2
#define V3 float3
#define DEFINIT
#endif
struct ChunkVertex {
	// used 16 bits
	// free 16 bits
	// uv X        - 7 bit 11
	// uv Y        - 7 bit 4
	// uv flip     - 2 bit 2
	// uv rotation - 2 bit 0
	u32 data0 DEFINIT;
	V3 position DEFINIT;
	u32 normal DEFINIT; // free 8 bits
	u32 tangent DEFINIT; // free 8 bits

#ifdef __cplusplus
	void setBits0(u32 val, u32 mask, u32 offset) {
		assert(val <= (mask));
		data0 = (data0 & ~(mask << offset)) | (val << offset);
	}
	void setUvRotation(u32 r) { setBits0(r, uvRotationMask, uvRotationOffset); }
	void setUvFlip(u32 r) { setBits0(r, uvFlipMask, uvFlipOffset); }
	auto& setData0(u32 u, u32 v) {
		data0 = makeVertexData0(u, v, 0, 0);
		return *this;
	}
	auto& setUv(V2i uv) {
		setBits0((uv.x << 7) | uv.y, (uvXMask << 7) | uvYMask, uvYOffset);
		return *this;
	}
#endif
};
#ifndef __cplusplus
#undef u32
#undef V2
#undef V3
#undef DEFINIT
static const float2x2 uvRots[4] = {
	float2x2(1,0,
			 0,1),
	float2x2(0,1,
			 -1,0),
	float2x2(-1,0,
			 0,-1),
	float2x2(0,-1,
			 1,0),
};
static const float2 uvFlips[4] = {
	float2(1,1),
	float2(1,-1),
	float2(-1,1),
	float2(-1,-1),
};
float4 getPosition(ChunkVertex v) {
	return float4(v.position, 1);
}
void getNormal(ChunkVertex v, out float3 normal, out float3 tangent, out float3 bitangent) {
	normal.x  = ((v.normal  >> 24) & 0xFF);
	normal.y  = ((v.normal  >> 16) & 0xFF);
	normal.z  = ((v.normal  >> 8 ) & 0xFF);
	tangent.x = ((v.tangent >> 24) & 0xFF);
	tangent.y = ((v.tangent >> 16) & 0xFF);
	tangent.z = ((v.tangent >> 8 ) & 0xFF);
	normal  = normalize(normal  / 127.5f - 1.f);
	tangent = normalize(tangent / 127.5f - 1.f);
	bitangent = cross(normal, tangent);
}
float2 getUv(ChunkVertex v, out float2x2 rot, out float2 flip) {
	float2 uv;
	uv.x = ((v.data0 >> uvXOffset) & uvXMask) * ATLAS_ENTRY_SIZE * 0.5f;
	uv.y = ((v.data0 >> uvYOffset) & uvYMask) * ATLAS_ENTRY_SIZE * 0.5f;
	rot = uvRots[(v.data0 >> uvRotationOffset) & uvRotationMask];
	flip = uvFlips[(v.data0 >> uvFlipOffset) & uvFlipMask];
	return uv;
}
#endif
#endif