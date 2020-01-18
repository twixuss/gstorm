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
struct BlockVertex {
	// free 11 bit
	// position X  - 5 bit
	// position Y  - 5 bit 
	// position Z  - 5 bit 
	// position ID - 3 bit
	// normal ID   - 3 bit
	u32 data0 DEFINIT;
	// free 20 bit
	// uv X - 6 bit
	// uv Y - 6 bit
	u32 data1 DEFINIT;

#ifdef __cplusplus
	inline static constexpr u32 positionIdOffset = 3;
	inline static constexpr u32 positionIdMask = 0b111 << positionIdOffset;
	void setPositionID(int pid) {
		assert(pid < 8);
		data0 &= ~positionIdMask;
		data0 |= pid << positionIdOffset;
	}
	void setData0(int px, int py, int pz, int pid, int n) {
		data0 = makeVertexData0(px, py, pz, pid, n);
	}
	void setData1(int u, int v) { 
		data1 = makeVertexData1(u, v);
	}
	void setData1(V2i uv) { setData1(uv.x, uv.y); }
#endif
};
#endif