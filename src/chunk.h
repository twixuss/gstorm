#pragma once
#include "common.h"
#include <numeric>
V3 r2w(V3 rel, V3i chunk) {
	return rel + (V3)(chunk * CHUNK_WIDTH);
}
V3i r2w(V3i rel, V3i chunk) {
	return rel + chunk * CHUNK_WIDTH;
}
V3 w2r(V3 world, V3i chunk) {
	return world - (V3)(chunk * CHUNK_WIDTH);
}
V3i w2r(V3i world, V3i chunk) {
	return world - chunk * CHUNK_WIDTH;
}
i64 generateTime, generateCount;
i64 buildMeshTime, buildMeshCount;
constexpr u32 maxChunkVertexCount = (CHUNK_WIDTH*CHUNK_WIDTH*(CHUNK_WIDTH+1)*3*4);
constexpr u32 maxChunkIndexCount  = (CHUNK_WIDTH*CHUNK_WIDTH*(CHUNK_WIDTH+1)*3*6);
std::atomic_uint meshesBuilt = 0;
struct Chunk;
using Neighbors = std::array<Chunk*, 6>;
using Blocks = std::array<BlockID, CHUNK_VOLUME>;
using BlockArena = Arena<Blocks, 0x100>;
struct Chunk {
	struct VertexBuffer {
		ID3D11Buffer* vBuffer = 0;
		ID3D11Buffer* iBuffer = 0;
		ID3D11ShaderResourceView* vBufferView = 0;
		ID3D11ShaderResourceView* iBufferView = 0;
		u32 vertexCount = 0;
		u32 indexCount = 0;
		void free() {
			if (vBuffer)     { vBuffer->Release();     vBuffer     = 0; }
			if (vBufferView) { vBufferView->Release(); vBufferView = 0; }
			if (iBuffer)     { iBuffer->Release();     iBuffer     = 0; }
			if (iBufferView) { iBufferView->Release(); iBufferView = 0; }
			vertexCount = 0;
			indexCount = 0;
		}
	};
	struct MeshBuffer {
		VertexBuffer buffers[2];
		VertexBuffer* current = buffers;
		VertexBuffer* next = buffers + 1;
		void swap() {
			std::swap(current, next);
		}
		void free() {
			for (auto& b : buffers) {
				b.free();
			}
		}
	};
	enum class State {
		unloaded,
		loaded, 
		buildingMesh, 
		wantedToBeDeleted
	};
	MeshBuffer meshBuffer;
	Blocks* blocks = 0;
	D3D11& renderer;
	const V3i position;
	std::atomic<State> state {State::unloaded};
	bool needToSave = false;
	bool generated = false;
	std::atomic_int userCount = 0;
	std::mutex bufferMutex;
	HANDLE usageSemaphore;
	Chunk(V3i position, D3D11& renderer) : position(position), renderer(renderer) {
		usageSemaphore = CreateSemaphoreExA(0, 1, 1, 0, 0, SYNCHRONIZE);
	}
	~Chunk() {
		assert(userCount == 0);
	}
	void allocateBlocks(BlockArena& arena) {
		if (!blocks) {
			blocks = arena.allocate().data;
		}
	}
	void freeBlocks(BlockArena& arena) {
		if (blocks) {
			arena.free(blocks);
			blocks = 0;
		}
	}
	void free(BlockArena& arena) {
		freeBlocks(arena);
		meshBuffer.free();
		//approxMesh.free();
	}
	FilePos filePos = FILEPOS_INVALID;
	void load(FILE* saveFile, FilePos where, BlockArena& arena) {
		filePos = where;

		assert(saveFile);
		_fseeki64(saveFile, filePos, SEEK_SET);

		allocateBlocks(arena);
		fread(blocks, CHUNK_SIZE, 1, saveFile);

		generated = true;
	}
	void save(FILE* saveFile, FilePos where) {
		filePos = where;
		needToSave = false;

		assert(saveFile);
		assert(filePos != FILEPOS_INVALID);
		_fseeki64(saveFile, filePos, SEEK_SET);

		assert(blocks);
		fwrite(blocks, CHUNK_SIZE, 1, saveFile);
	}
	// returns true if block changed
	bool setBlock(int x, int y, int z, BlockID blk) {
		assert(x >= 0 && x < CHUNK_WIDTH);
		assert(y >= 0 && y < CHUNK_WIDTH);
		assert(z >= 0 && z < CHUNK_WIDTH);
		needToSave = true;
		auto& b = blocks->at(BLOCK_INDEX(x, y, z));
		auto result = b != blk;
		b = blk;
		return result;
	}
	bool setBlock(V3i p, BlockID blk) {
		return setBlock(p.x, p.y, p.z, blk);
	}
	BlockID& getBlock(int x, int y, int z) {
		assert(x >= 0 && x < CHUNK_WIDTH);
		assert(y >= 0 && y < CHUNK_WIDTH);
		assert(z >= 0 && z < CHUNK_WIDTH);
		assert(blocks);
		return blocks->at(BLOCK_INDEX(x, y, z));
	}
	BlockID& getBlock(V3i p) {
		return getBlock(p.x, p.y, p.z);
	}
	void generate(BlockArena& arena) {
		//TIMED_FUNCTION_;
		allocateBlocks(arena);
		assert(!generated);
		//printf("Generated ");
		//printChunk(blocks, position);
		if (position.y < 0) {
			memset(blocks, 0x01010101, CHUNK_SIZE);
		}
		else {
			auto beginCounter = WH::getPerformanceCounter();
			for (int x = 0; x < CHUNK_WIDTH; ++x) {
				for (int z = 0; z < CHUNK_WIDTH; ++z) {
					V2i globalPos = {position.x * CHUNK_WIDTH + x, position.z * CHUNK_WIDTH + z};
					int h = 0;
					//h += (int)(128 - perlin(seed + 100000, 8) * 256);
					h += (int)(textureDetail(8, 0.5f, globalPos + V2i {1241, 6177}, 512, [](V2i pos, i32 s) -> f32 {
						return voronoi(pos, s);
											 }) * 512) + 2;
					h -= position.y * CHUNK_WIDTH;
					auto top = h;
					h = clamp(h, 0, CHUNK_WIDTH);
					for (int y = 0; y < h; ++y) {
						auto b = BLOCK_DIRT;
						if (y == top - 1) {
							//b = BLOCK_TALL_GRASS;
							b = textureDetail(2, 0.5f, globalPos, 8, [](V2i pos, i32 s) {
								return interpolate(pos, s, [](auto a, auto b, auto t) { return lerp(a,b,cos01(t)); }, [](V2i pos) {
									return noise(pos);
												   });
											  }) > 0.5f ? BLOCK_TALL_GRASS : BLOCK_AIR;
						}
						if (y == top - 2)
							b = BLOCK_GRASS;
						blocks->at(BLOCK_INDEX(x, y, z)) = b;
					}
				}
			}
			generateTime += WH::getPerformanceCounter() - beginCounter;
			++generateCount;
		}
		generated = true;
	}
	void generateMesh(ChunkVertex* vertexPool, u32* indexPool, const Neighbors& neighbors) {
		{
			State expectedState = State::loaded;
			if (!state.compare_exchange_strong(expectedState, State::buildingMesh)) {
				assert(expectedState == State::wantedToBeDeleted);
				return;
			}
		}
		DEFER {
			State expectedState = State::buildingMesh;
			assert(state.compare_exchange_strong(expectedState, State::loaded));
		};
		assert(blocks);
		ChunkVertex* verticesBegin = vertexPool;
		u32* indicesBegin = indexPool;
		auto pushVertex = [&vertexPool, verticesBegin](ChunkVertex vert) {
			assert(vertexPool - verticesBegin < maxChunkVertexCount && "small pool");
			*vertexPool++ = vert;
		};
		auto pushIndex = [&indexPool, indicesBegin](u32 idx) {
			assert(indexPool - indicesBegin < maxChunkIndexCount && "small pool");
			*indexPool++ = idx;
		};
		auto pushVertices = [&vertexPool, verticesBegin](const auto& list) {
			assert((size_t)maxChunkVertexCount - (vertexPool - verticesBegin) >= std::size(list) && "small pool");
			std::copy(std::begin(list), std::end(list), vertexPool);
			vertexPool += std::size(list);
		};
		auto pushIndices = [&indexPool, indicesBegin](std::initializer_list<u32> list) {
			assert((size_t)maxChunkIndexCount - (indexPool - indicesBegin) >= list.size() && "small pool");
			std::copy(list.begin(), list.end(), indexPool);
			indexPool += list.size();
		};
		u32 idx = 0;
		auto beginCounter = WH::getPerformanceCounter();
		//u32 idxOff = 0;
		FOR_BLOCK_IN_CHUNK {
			auto & b = getBlock(x,y,z);
			switch (b) {
				case 0:
					continue;
			}
			auto& blockInfo = blockInfos.at(b);

			static constexpr V3 cornerOffsets[8]{
				{ 0.5, 0.5, 0.5},
				{ 0.5, 0.5,-0.5},
				{ 0.5,-0.5, 0.5},
				{ 0.5,-0.5,-0.5},
				{-0.5, 0.5, 0.5},
				{-0.5, 0.5,-0.5},
				{-0.5,-0.5, 0.5},
				{-0.5,-0.5,-0.5},
			};
			static constexpr u32 normals[6]{
				0xFF808000,
				0x00808000,
				0x80FF8000,
				0x80008000,
				0x8080FF00,
				0x80800000,
			};
			static constexpr u32 tangents[6]{
				0x80800000,
				0x8080FF00,
				0x00808000,
				0x00808000,
				0xFF808000,
				0x00808000,
			};
			switch (blockInfo.type) {
				case BlockInfo::Type::default:
				case BlockInfo::Type::topSideBottom: {
					bool visible[6];
					visible[AXIS_PX] = x == CHUNK_WIDTH - 1 ? (neighbors[0] ? isTransparent(neighbors[0]->getBlock(0, y, z              )) : true) : isTransparent(getBlock(x + 1, y, z));
					visible[AXIS_NX] =               x == 0 ? (neighbors[1] ? isTransparent(neighbors[1]->getBlock(CHUNK_WIDTH - 1, y, z)) : true) : isTransparent(getBlock(x - 1, y, z));
					visible[AXIS_PY] = y == CHUNK_WIDTH - 1 ? (neighbors[2] ? isTransparent(neighbors[2]->getBlock(x,               0, z)) : true) : isTransparent(getBlock(x, y + 1, z));
					visible[AXIS_NY] =               y == 0 ? (neighbors[3] ? isTransparent(neighbors[3]->getBlock(x, CHUNK_WIDTH - 1, z)) : true) : isTransparent(getBlock(x, y - 1, z));
					visible[AXIS_PZ] = z == CHUNK_WIDTH - 1 ? (neighbors[4] ? isTransparent(neighbors[4]->getBlock(x, y,               0)) : true) : isTransparent(getBlock(x, y, z + 1));
					visible[AXIS_NZ] =               z == 0 ? (neighbors[5] ? isTransparent(neighbors[5]->getBlock(x, y, CHUNK_WIDTH - 1)) : true) : isTransparent(getBlock(x, y, z - 1));
					auto calcVerts = [&](u8 axis) {
						u32 defData0 = makeVertexData0(0,0,0,0);
						ChunkVertex verts[4] {};
						verts[0].position = V3 {V3i{x, y, z}};
						verts[1].position = V3 {V3i{x, y, z}};
						verts[2].position = V3 {V3i{x, y, z}};
						verts[3].position = V3 {V3i{x, y, z}};
						verts[0].normal = normals[axis];
						verts[1].normal = normals[axis];
						verts[2].normal = normals[axis];
						verts[3].normal = normals[axis];
						verts[0].tangent = tangents[axis];
						verts[1].tangent = tangents[axis];
						verts[2].tangent = tangents[axis];
						verts[3].tangent = tangents[axis];
						verts[0].data0 = defData0;
						verts[1].data0 = defData0;
						verts[2].data0 = defData0;
						verts[3].data0 = defData0;
						switch (axis) {
							case AXIS_PX:
								verts[0].position += cornerOffsets[1];
								verts[1].position += cornerOffsets[0];
								verts[2].position += cornerOffsets[3];
								verts[3].position += cornerOffsets[2];
								break;
							case AXIS_NX:
								verts[0].position += cornerOffsets[4];
								verts[1].position += cornerOffsets[5];
								verts[2].position += cornerOffsets[6];
								verts[3].position += cornerOffsets[7];
								break;
							case AXIS_PY:
								verts[0].position += cornerOffsets[4];
								verts[1].position += cornerOffsets[0];
								verts[2].position += cornerOffsets[5];
								verts[3].position += cornerOffsets[1];
								break;
							case AXIS_NY:
								verts[0].position += cornerOffsets[7];
								verts[1].position += cornerOffsets[3];
								verts[2].position += cornerOffsets[6];
								verts[3].position += cornerOffsets[2];
								break;
							case AXIS_PZ:
								verts[0].position += cornerOffsets[0];
								verts[1].position += cornerOffsets[4];
								verts[2].position += cornerOffsets[2];
								verts[3].position += cornerOffsets[6];
								break;
							case AXIS_NZ:
								verts[0].position += cornerOffsets[5];
								verts[1].position += cornerOffsets[1];
								verts[2].position += cornerOffsets[7];
								verts[3].position += cornerOffsets[3];
								break;
							default:
								assert(0);
						}
						int pos = blockInfo.offsetAtlasPos(axis);
						V2i atlasPos = 2 * V2i {
							pos % ATLAS_SIZE,
							pos / ATLAS_SIZE,
						};
						u32 uvIdx = 0;
						if (blockInfo.randomizeUv[axis]) {
							uvIdx = randomU32(r2w(V3i {x,y,z}, position)) % 4;
						}
						verts[0].setUvRotation(uvIdx);
						verts[1].setUvRotation(uvIdx);
						verts[2].setUvRotation(uvIdx);
						verts[3].setUvRotation(uvIdx);
						V2i uvs[4];
						uvs[0] = {atlasPos.x,  atlasPos.y};
						uvs[1] = {atlasPos.x + 2,atlasPos.y};
						uvs[2] = {atlasPos.x + 2,atlasPos.y + 2};
						uvs[3] = {atlasPos.x,  atlasPos.y + 2};
						verts[0].setUv(uvs[(uvIdx + 0) % 4]);
						verts[1].setUv(uvs[(uvIdx + 1) % 4]);
						verts[2].setUv(uvs[(uvIdx + 3) % 4]);
						verts[3].setUv(uvs[(uvIdx + 2) % 4]);
						pushVertices(verts);
						pushIndices({idx + 0,
									 idx + 1,
									 idx + 2,
									 idx + 1,
									 idx + 3,
									 idx + 2});
						idx += 4;
					};
					int visibleCount = std::accumulate(std::begin(visible), std::end(visible), 0);
					int visibleBits = (visible[0] << 5) | (visible[1] << 4) | (visible[2] << 3) | (visible[3] << 2) | (visible[4] << 1) | (visible[5] << 0);
					if (visibleCount == 0) {
						break;
					}
					else if (visibleCount == 1) {
						calcVerts([&] { for (u8 i = 0; i < 6; ++i) if (visible[i]) return i; assert(0); }());
						break;
					}
					else if (visibleCount == 6) {
						ChunkVertex verts[6] {};
						verts[0].normal = 0xFF808000;
						verts[1].normal = 0x00808000;
						verts[2].normal = 0x80FF8000;
						verts[3].normal = 0x80008000;
						verts[4].normal = 0x8080FF00;
						verts[5].normal = 0x80800000;
						verts[0].tangent = 0x8080FF00;
						verts[1].tangent = 0x80800000;
						verts[2].tangent = 0xFF808000;
						verts[3].tangent = 0xFF808000;
						verts[4].tangent = 0x00808000;
						verts[5].tangent = 0xFF808000;
						verts[0].position = V3 {V3i{x, y, z}} + V3{0.5,0,0};
						verts[1].position = V3 {V3i{x, y, z}} + V3{-0.5,0,0};
						verts[2].position = V3 {V3i{x, y, z}} + V3{0,0.5,0};
						verts[3].position = V3 {V3i{x, y, z}} + V3{0,-0.5,0};
						verts[4].position = V3 {V3i{x, y, z}} + V3{0,0,0.5};
						verts[5].position = V3 {V3i{x, y, z}} + V3{0,0,-0.5};
						pushVertices(verts);
						pushIndices({idx + 2, idx + 4, idx + 0,
									 idx + 2, idx + 1, idx + 4,
									 idx + 2, idx + 5, idx + 1,
									 idx + 2, idx + 0, idx + 5,
									 idx + 3, idx + 0, idx + 4,
									 idx + 3, idx + 4, idx + 1,
									 idx + 3, idx + 1, idx + 5,
									 idx + 3, idx + 5, idx + 0});

						idx += 6;
						break;
					}
					else if (0) {
						ChunkVertex verts[4] {};
						auto getNormal = [visibleBits]() {
							switch (visibleBits) {
								case 0b101000: return 0xFFFF8000u; //+x+y
								case 0b100100: return 0xFF008000u; //+x-y
								case 0b100010: return 0xFF80FF00u; //+x+z
								case 0b100001: return 0xFF800000u; //+x-z
								case 0b011000: return 0x00FF8000u; //-x+y
								case 0b010100: return 0x00008000u; //-x-y
								case 0b010010: return 0x0080FF00u; //-x+z
								case 0b010001: return 0x00800000u; //-x-z
								case 0b001010: return 0x80FFFF00u; //+y+z
								case 0b001001: return 0x80FF0000u; //+y-z
								case 0b000110: return 0x8000FF00u; //-y+z
								case 0b000101: return 0x80000000u; //-y-z
							}
							assert(0);
						};
						verts[0].normal = 0xFFFF8000;
						verts[1].normal = 0xFFFF8000;
						verts[2].normal = 0xFFFF8000;
						verts[3].normal = 0xFFFF8000;
						verts[0].tangent = 0x8080FF00;
						verts[1].tangent = 0x8080FF00;
						verts[2].tangent = 0x8080FF00;
						verts[3].tangent = 0x8080FF00;
						verts[0].position = V3 {V3i{x, y, z}} + cornerOffsets[3];
						verts[1].position = V3 {V3i{x, y, z}} + cornerOffsets[5];
						verts[2].position = V3 {V3i{x, y, z}} + cornerOffsets[2];
						verts[3].position = V3 {V3i{x, y, z}} + cornerOffsets[4];
						int pos = blockInfo.offsetAtlasPos(AXIS_PY);
						V2i atlasPos = 2 * V2i {
							pos % ATLAS_SIZE,
							pos / ATLAS_SIZE,
						};
						u32 uvIdx = 0;
						if (blockInfo.randomizeUv[0]) {
							uvIdx = randomU32(r2w(V3i {x,y,z}, position)) % 4;
						}
						verts[0].setUvRotation(uvIdx);
						verts[1].setUvRotation(uvIdx);
						verts[2].setUvRotation(uvIdx);
						verts[3].setUvRotation(uvIdx);
						V2i uvs[4];
						uvs[0] = {atlasPos.x,  atlasPos.y};
						uvs[1] = {atlasPos.x + 2,atlasPos.y};
						uvs[2] = {atlasPos.x + 2,atlasPos.y + 2};
						uvs[3] = {atlasPos.x,  atlasPos.y + 2};
						verts[0].setUv(uvs[(uvIdx + 0) % 4]);
						verts[1].setUv(uvs[(uvIdx + 1) % 4]);
						verts[2].setUv(uvs[(uvIdx + 3) % 4]);
						verts[3].setUv(uvs[(uvIdx + 2) % 4]);
						pushVertices(verts);
						pushIndices({idx + 0,
									 idx + 1,
									 idx + 2,
									 idx + 1,
									 idx + 3,
									 idx + 2});
						idx += 4;
						break;
					}
					/*
					if (visible[0] && !visible[1] && visible[2] && !visible[3] && visible[4] && !visible[5]) {
						ChunkVertex verts[6] {};
						verts[0].normal = 0xFFFF8000;
						verts[1].normal = 0xFFFFFF00;
						verts[2].normal = 0xFFFFFF00;
						verts[3].normal = 0x80FFFF00;
						verts[0].tangent = 0x8080FF00;
						verts[1].tangent = 0x00800000;
						verts[2].tangent = 0x00800000;
						verts[3].tangent = 0x00808000;
						verts[0].position = V3 {V3i{x, y, z}} +cornerOffsets[3];
						verts[1].position = V3 {V3i{x, y, z}} +cornerOffsets[5];
						verts[2].position = V3 {V3i{x, y, z}} +cornerOffsets[2];
						verts[3].position = V3 {V3i{x, y, z}} +cornerOffsets[6];
						int pos = blockInfo.offsetAtlasPos(AXIS_PY);
						V2i atlasPos {
							pos % ATLAS_SIZE,
							pos / ATLAS_SIZE,
						};
						u32 uvIdx = 0;
						if (blockInfo.randomizeUv[0]) {
							uvIdx = randomU32(r2w(V3i {x,y,z}, position)) % 4;
						}
						verts[0].setUvRotation(uvIdx);
						verts[1].setUvRotation(uvIdx);
						verts[2].setUvRotation(uvIdx);
						verts[3].setUvRotation(uvIdx);
						V2i uvs[4];
						uvs[0] = {atlasPos.x,  atlasPos.y};
						uvs[1] = {atlasPos.x + 1,atlasPos.y};
						uvs[2] = {atlasPos.x + 1,atlasPos.y + 1};
						uvs[3] = {atlasPos.x,  atlasPos.y + 1};
						verts[0].setUv(uvs[(uvIdx + 0) % 4]);
						verts[1].setUv(uvs[(uvIdx + 1) % 4]);
						verts[2].setUv(uvs[(uvIdx + 3) % 4]);
						verts[3].setUv(uvs[(uvIdx + 2) % 4]);
						pushVertex(verts[0]);
						pushVertex(verts[1]);
						pushVertex(verts[2]);
						pushVertex(verts[3]);
						pushIndex(idx + 0);
						pushIndex(idx + 1);
						pushIndex(idx + 2);
						pushIndex(idx + 1);
						pushIndex(idx + 3);
						pushIndex(idx + 2);
						idx += 4;
						break;
					}
					*/
					if (visible[AXIS_PX]) calcVerts(AXIS_PX);
					if (visible[AXIS_NX]) calcVerts(AXIS_NX);
					if (visible[AXIS_PY]) calcVerts(AXIS_PY);
					if (visible[AXIS_NY]) calcVerts(AXIS_NY);
					if (visible[AXIS_PZ]) calcVerts(AXIS_PZ);
					if (visible[AXIS_NZ]) calcVerts(AXIS_NZ);
					break;
				}
				case BlockInfo::Type::x: {
					ChunkVertex verts[4] {};
					V2i atlasPos = 2 * V2i {
						 blockInfo.atlasPos % ATLAS_SIZE,
						 blockInfo.atlasPos / ATLAS_SIZE,
					};
					V2i uvs[4];
					uvs[0] = {atlasPos.x,  atlasPos.y};
					uvs[1] = {atlasPos.x + 2,atlasPos.y};
					uvs[2] = {atlasPos.x + 2,atlasPos.y + 2};
					uvs[3] = {atlasPos.x,  atlasPos.y + 2};
					auto randomizeUv = [&](u32 o) { // o is different for each face
						if (randomU32(r2w(V3i {x,y,z} + o, position)) & 1) {
							verts[0].setUv(uvs[0]);
							verts[1].setUv(uvs[1]);
							verts[2].setUv(uvs[3]);
							verts[3].setUv(uvs[2]);
						}
						else {
							verts[0].setUv(uvs[1]);//.setUvFlip(0b10);
							verts[1].setUv(uvs[0]);//.setUvFlip(0b10);
							verts[2].setUv(uvs[2]);//.setUvFlip(0b10);
							verts[3].setUv(uvs[3]);//.setUvFlip(0b10);
							verts[0].tangent = invertRGBA(verts[0].tangent);
							verts[1].tangent = invertRGBA(verts[1].tangent);
							verts[2].tangent = invertRGBA(verts[2].tangent);
							verts[3].tangent = invertRGBA(verts[3].tangent);
						}
					};
					auto insertVertices = [&](u32 diag, u32 flip) {
						verts[0].position = V3{V3i{x, y, z}} +cornerOffsets[0 + diag];
						verts[1].position = V3{V3i{x, y, z}} +cornerOffsets[5 - diag];
						verts[2].position = V3{V3i{x, y, z}} +cornerOffsets[2 + diag];
						verts[3].position = V3{V3i{x, y, z}} +cornerOffsets[7 - diag];
						verts[0].normal = normals[AXIS_PY];
						verts[1].normal = normals[AXIS_PY];
						verts[2].normal = normals[AXIS_PY];
						verts[3].normal = normals[AXIS_PY];
						verts[0].tangent = tangents[AXIS_PY];
						verts[1].tangent = tangents[AXIS_PY];
						verts[2].tangent = tangents[AXIS_PY];
						verts[3].tangent = tangents[AXIS_PY];
						verts[0].data0 = makeVertexData0(0, 0, 0, 0);
						verts[1].data0 = makeVertexData0(0, 0, 0, 0);
						verts[2].data0 = makeVertexData0(0, 0, 0, 0);
						verts[3].data0 = makeVertexData0(0, 0, 0, 0);
						randomizeUv((diag << 1) | flip);
						pushVertex(verts[0]);
						pushVertex(verts[1]);
						pushVertex(verts[2]);
						pushVertex(verts[3]);
						if (flip) {
							pushIndex(idx + 0);
							pushIndex(idx + 1);
							pushIndex(idx + 2);
							pushIndex(idx + 1);
							pushIndex(idx + 3);
							pushIndex(idx + 2);
						}
						else {
							pushIndex(idx + 0);
							pushIndex(idx + 2);
							pushIndex(idx + 1);
							pushIndex(idx + 1);
							pushIndex(idx + 2);
							pushIndex(idx + 3);
						}
						idx += 4;
					};
					insertVertices(0, 0);
					insertVertices(0, 1);
					insertVertices(1, 0);
					insertVertices(1, 1);
					break;
				}
									   /*
				case BlockInfo::Type::water: {
					bool visible = y == CHUNK_WIDTH - 1 ? (neighbors[2] ? neighbors[2]->getBlock(x, 0, z) == BLOCK_AIR : true) : getBlock(x, y + 1, z) == BLOCK_AIR;
					if (!visible)
						break;
					u32 defData0 = makeVertexData0(0, 0, 0, 0, AXIS_PY);
					ChunkVertex verts[4] {};
					verts[0].position = V3 {V3i{x, y, z}};
					verts[1].position = V3 {V3i{x, y, z}};
					verts[2].position = V3 {V3i{x, y, z}};
					verts[3].position = V3 {V3i{x, y, z}};
					verts[0].data0 = defData0;
					verts[1].data0 = defData0;
					verts[2].data0 = defData0;
					verts[3].data0 = defData0;
					verts[0].position += cornerOffsets[4];
					verts[1].position += cornerOffsets[0];
					verts[2].position += cornerOffsets[5];
					verts[3].position += cornerOffsets[1];
					int pos = blockInfo.offsetAtlasPos(AXIS_PY);
					V2i atlasPos {
						pos % ATLAS_SIZE,
						pos / ATLAS_SIZE,
					};
					u32 uvIdx = 0;
					if (blockInfo.randomizeUv[AXIS_PY]) {
						uvIdx = randomU32(r2w(V3i {x,y,z}, position)) % 4;
					}
					verts[0].setUvRotation(uvIdx);
					verts[1].setUvRotation(uvIdx);
					verts[2].setUvRotation(uvIdx);
					verts[3].setUvRotation(uvIdx);
					V2i uvs[4];
					uvs[0] = {atlasPos.x,  atlasPos.y};
					uvs[1] = {atlasPos.x + 1,atlasPos.y};
					uvs[2] = {atlasPos.x + 1,atlasPos.y + 1};
					uvs[3] = {atlasPos.x,  atlasPos.y + 1};
					verts[0].setUv(uvs[(uvIdx + 0) % 4]);
					verts[1].setUv(uvs[(uvIdx + 1) % 4]);
					verts[2].setUv(uvs[(uvIdx + 3) % 4]);
					verts[3].setUv(uvs[(uvIdx + 2) % 4]);
					pushVertex(verts[0]);
					pushVertex(verts[1]);
					pushVertex(verts[2]);
					pushVertex(verts[1]);
					pushVertex(verts[3]);
					pushVertex(verts[2]);
					break;
				}
				*/
				default:
					assert(0);
			}
		}
		buildMeshTime += WH::getPerformanceCounter() - beginCounter;
		++buildMeshCount;

		std::unique_lock l(bufferMutex);
		auto& buffer = *meshBuffer.next;

		buffer.free();

		buffer.vertexCount = (u32)(vertexPool - verticesBegin);
		buffer.indexCount = (u32)(indexPool - indicesBegin);

		if (!buffer.vertexCount || !buffer.indexCount)
			return;

		u32 vertexBufferSize = buffer.vertexCount * sizeof(verticesBegin[0]);
		u32 indexBufferSize = buffer.indexCount * sizeof(indicesBegin[0]);
		buffer.vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, sizeof(verticesBegin[0]), verticesBegin);
		buffer.iBuffer = renderer.createImmutableStructuredBuffer(indexBufferSize, sizeof(indicesBegin[0]), indicesBegin);

		buffer.vBufferView = renderer.createShaderResourceView(buffer.vBuffer, D3D11_SRV_DIMENSION_BUFFER, buffer.vertexCount);
		buffer.iBufferView = renderer.createShaderResourceView(buffer.iBuffer, D3D11_SRV_DIMENSION_BUFFER, buffer.indexCount);
		++meshesBuilt;
	}
	void draw(D3D11::ConstantBuffer<DrawCBuffer>& drawCBuffer, V3i playerChunk, const M4& matrixVP, const M4& matrixCamPos) {
		auto buffer = meshBuffer.current;
		{
			std::unique_lock lk(bufferMutex);
			if (meshBuffer.next->vBuffer) {
				meshBuffer.current->free();
				meshBuffer.swap();
				buffer = meshBuffer.current;
			}
			if (!buffer->vertexCount || !buffer->vBufferView)
				return;
			if (!buffer->indexCount || !buffer->iBufferView)
				return;
			renderer.vsSetShaderResource(0, buffer->vBufferView);
			renderer.vsSetShaderResource(1, buffer->iBufferView);
		}
		drawCBuffer.model = M4::translation((V3)((position - playerChunk) * CHUNK_WIDTH));
		drawCBuffer.modelCamPos = matrixCamPos * drawCBuffer.model;
		drawCBuffer.mvp = matrixVP * drawCBuffer.model;
		drawCBuffer.update(renderer);
		renderer.draw(buffer->indexCount);
	}
};