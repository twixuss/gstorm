#ifndef _BLOCK_VERTEX_H_
#define _BLOCK_VERTEX_H_
#define ATLAS_SIZE 32
#define ATLAS_ENTRY_SIZE (1.0f/ATLAS_SIZE)
#ifdef __cplusplus
u32 makeVertexData0(int px, int py, int pz, int pid, int uf, int ur, int n) {
	assert(px < CHUNK_WIDTH);
	assert(py < CHUNK_WIDTH);
	assert(pz < CHUNK_WIDTH);
	assert(pid < 8);
	assert(n < 8);
	static_assert(CHUNK_WIDTH == 64);
	return (px << 22) | (py << 16) | (pz << 10) | (pid << 7) | (uf << 5) | (ur << 3) | n;
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
#define positionXOffset  22
#define positionYOffset  16
#define positionZOffset  10
#define positionIdOffset 7
#define uvRotationOffset 3
#define uvFlipOffset     5
#define normalIdOffset   0

#define positionMask   0x3F
#define positionIdMask 0x7
#define uvRotationMask 0x3
#define uvFlipMask     0x3
#define normalIdMask   0x3

#define uvXOffset 6
#define uvYOffset 0

#define uvXMask 0x3F
#define uvYMask 0x3F

struct ChunkVertex {
	// free 4 bit
	// position X  - 6 bit 22
	// position Y  - 6 bit 16
	// position Z  - 6 bit 10
	// position ID - 3 bit 7
	// uv flip     - 2 bit 5
	// uv rotation - 2 bit 3
	// normal ID   - 3 bit 0
	u32 data0 DEFINIT;
	// free 20 bit
	// uv X - 6 bit 6
	// uv Y - 6 bit 0
	u32 data1 DEFINIT;

#ifdef __cplusplus
	void setBits0(u32 val, u32 mask, u32 offset) {
		assert(val <= (mask));
		data0 = (data0 & ~(mask << offset)) | (val << offset);
	}
	void setPositionID(u32 pid) { setBits0(pid, positionIdMask, positionIdOffset); }
	void setUvRotation(u32 r) { setBits0(r, uvRotationMask, uvRotationOffset); }
	void setUvFlip(u32 r) { setBits0(r, uvFlipMask, uvFlipOffset); }
	auto& setData0(u32 px, u32 py, u32 pz, u32 pid, u32 n) {
		data0 = makeVertexData0(px, py, pz, pid, 0, 0, n);
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
	float4 position = float4(positions[(v.data0 >> positionIdOffset) & positionIdMask], 1);
	position.x += (v.data0 >> positionXOffset) & positionMask;
	position.y += (v.data0 >> positionYOffset) & positionMask;
	position.z += (v.data0 >> positionZOffset) & positionMask;
	return position;
}
void getNormal(ChunkVertex v, out float3 normal, out float3 tangent, out float3 bitangent) {
	uint idx = (v.data0 >> normalIdOffset) & normalIdMask;
	tangent= tangents[idx];
	bitangent = bitangents[idx];
	normal = normals[idx];
}
float2 getUv(ChunkVertex v, out float2x2 rot, out float2 flip) {
	float2 uv;
	uv.x = ((v.data1 >> uvXOffset) & uvXMask) * ATLAS_ENTRY_SIZE;
	uv.y = ((v.data1 >> uvYOffset) & uvYMask) * ATLAS_ENTRY_SIZE;
	rot = uvRots[(v.data0 >> uvRotationOffset) & uvRotationMask];
	flip = uvFlips[(v.data0 >> uvFlipOffset) & uvFlipMask];
	return uv;
}
#endif
#endif