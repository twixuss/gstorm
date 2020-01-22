#ifndef _BLOCK_VERTEX_H_
#define _BLOCK_VERTEX_H_
#define ATLAS_SIZE 32
#define ATLAS_ENTRY_SIZE (1.0f/ATLAS_SIZE)
#ifdef __cplusplus
u32 makeVertexData0(int px, int py, int pz, int pid, int n) {
	assert(px < 32);
	assert(py < 32);
	assert(pz < 32);
	assert(pid < 8);
	assert(n < 8);
	return (px << 16) | (py << 11) | (pz << 6) | (pid << 3) | n;
}
u32 makeVertexData1(int u, int v) {
	assert(u <= ATLAS_SIZE);
	assert(v <= ATLAS_SIZE);
	return (u << 6) | v;
}
#define DEFINIT {}
#else
#define u32 uint
#define V2 float2
#define DEFINIT
#endif
struct ChunkVertex {
	// free 5 bit
	// uv flip     - 2 bit 23
	// uv rotation - 2 bit 21
	// position X  - 5 bit 16
	// position Y  - 5 bit 11
	// position Z  - 5 bit 6
	// position ID - 3 bit 3
	// normal ID   - 3 bit 0
	u32 data0 DEFINIT;
	// free 20 bit
	// uv X - 6 bit
	// uv Y - 6 bit
	u32 data1 DEFINIT;

#ifdef __cplusplus
	inline static constexpr u32 positionIdOffset = 3;
	inline static constexpr u32 positionIdMask = 0b111 << positionIdOffset;
	inline static constexpr u32 uvRotationOffset = 21;
	inline static constexpr u32 uvRotationMask = 0b11 << uvRotationOffset;
	inline static constexpr u32 uvFlipOffset = 23;
	inline static constexpr u32 uvFlipMask = 0b11 << uvFlipOffset;
	void setBits0(u32 val, u32 mask, u32 offset) {
		assert(val <= (mask >> offset));
		data0 = (data0 & ~mask) | (val << offset);
	}
	void setPositionID(u32 pid) { setBits0(pid, positionIdMask, positionIdOffset); }
	void setUvRotation(u32 r) { setBits0(r, uvRotationMask, uvRotationOffset); }
	void setUvFlip(u32 r) { setBits0(r, uvFlipMask, uvFlipOffset); }
	auto& setData0(u32 px, u32 py, u32 pz, u32 pid, u32 n) {
		data0 = makeVertexData0(px, py, pz, pid, n);
		return *this;
	}
	auto& setData1(u32 u, u32 v) {
		data1 = makeVertexData1(u, v);
		return *this;
	}
	auto& setData1(V2i uv) { return setData1(uv.x, uv.y); }
#endif
};
#ifndef __cplusplus
static const float3 normals[6] = {
	float3(1,0,0),
	float3(-1,0,0),
	float3(0, 1,0),
	float3(0,-1,0),
	float3(0,0, 1),
	float3(0,0,-1),
};
static const float3 tangents[6] = {
	float3(0,0,-1),
	float3(0,0,1),
	float3(-1,0,0),
	float3(-1,0,0),
	float3(1,0,0),
	float3(-1,0,0),
};
static const float3 bitangents[6] = {
	normalize(cross(normals[0], tangents[0])),
	normalize(cross(normals[1], tangents[1])),
	normalize(cross(normals[2], tangents[2])),
	normalize(cross(normals[3], tangents[3])),
	normalize(cross(normals[4], tangents[4])),
	normalize(cross(normals[5], tangents[5])),
};
static const float3 positions[8] = {
	float3(0.5, 0.5, 0.5),
	float3(0.5, 0.5,-0.5),
	float3(0.5,-0.5, 0.5),
	float3(0.5,-0.5,-0.5),
	float3(-0.5, 0.5, 0.5),
	float3(-0.5, 0.5,-0.5),
	float3(-0.5,-0.5, 0.5),
	float3(-0.5,-0.5,-0.5),
};
float4 getPosition(ChunkVertex v) {
	float4 position = float4(positions[(v.data0 >> 3) & 0x7], 1);
	position.x += (v.data0 >> 16) & 0x1F;
	position.y += (v.data0 >> 11) & 0x1F;
	position.z += (v.data0 >> 6) & 0x1F;
	return position;
}
void getNormal(ChunkVertex v, out float3 normal, out float3 tangent, out float3 bitangent) {
	uint idx = v.data0 & 0x7;
	tangent= tangents[idx];
	bitangent = bitangents[idx];
	normal = normals[idx];
}
float2 getUv(ChunkVertex v, out float2x2 rot, out float2 flip) {
	float2 uv;
	uv.x = ((v.data1 >> 6) & 0x3F)* ATLAS_ENTRY_SIZE;
	uv.y = (v.data1 & 0x3F) * ATLAS_ENTRY_SIZE;
	const float2x2 rots[4] = {
		float2x2(1,0,
				 0,1),
		float2x2(0,1,
				 -1,0),
		float2x2(-1,0,
				 0,-1),
		float2x2(0,-1,
				 1,0),
	};
	const float2 flips[4] = {
		float2(1,1),
		float2(1,-1),
		float2(-1,1),
		float2(-1,-1),
	};
	rot = rots[(v.data0 >> 21) & 0x3];
	flip = flips[(v.data0 >> 23) & 0x3];
	return uv;
}
#endif
#endif