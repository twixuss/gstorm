

// TODO: free memory of far chunks, but left them visible


#pragma warning(disable:4189)//unused
#pragma warning(disable:4324)//align padding
#pragma warning(disable:4359)//align smaller
#pragma warning(disable:4201)//no name
#include <time.h>
#include <stdio.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <functional>
#include <optional>
#include <array>
#include "common.h"
#include "math.h"
#include "math.cpp"
#include "winhelper.h"
#include "d3d11helper.h"
#include <timeapi.h>
#include <atomic>
#pragma comment(lib, "winmm")
#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_VISIBLE)
#define DATA "../data/"
#define LDATA L"../data/"
#define INVALID_BLOCK_POS INT32_MIN
#define FILEPOS_NOT_LOADED ((FilePos)-1)
#define FILEPOS_APPEND ((FilePos)-2)
#include "borismap.h"
#define WORLD_SURFACE 1
struct InputState {
	V2i mousePosition;
	V2i mouseDelta;
	bool keys[256] {};
	bool mouse[8] {};
};
struct Input {
	InputState current;
	InputState previous;
	void swap() {
		memcpy(&previous, &current, sizeof(current));
		current.mouseDelta = {};
	}
	bool keyHeld(u8 k) { return current.keys[k]; }
	bool keyDown(u8 k) { return current.keys[k] && !previous.keys[k]; }
	bool keyUp(u8 k) { return !current.keys[k] && previous.keys[k]; }
	bool mouseHeld(u8 k) { return current.mouse[k]; }
	bool mouseDown(u8 k) { return current.mouse[k] && !previous.mouse[k]; }
	bool mouseUp(u8 k) { return !current.mouse[k] && previous.mouse[k]; }
	auto mousePosition() { return current.mousePosition; }
	auto mouseDelta() { return current.mouseDelta; }
};
struct Window {
	HWND hwnd = 0;
	V2i clientSize;
	bool running = true;
	bool resize = true;
	bool killFocus = false;
};
// This queue can be used by 2 threads
// Thread #1 should only push
// Thread #2 should process and clear current buffer, then swap the pointers
template<class T>
struct DoubleQueue {
	void push(T chunk) {
		std::unique_lock l(mutex);
		next->push_back(chunk);
	}
	void swap() {
		std::unique_lock l(mutex);
		std::swap(current, next);
	}
	bool empty() { 
		if (current->empty()) {
			swap();
			if (current->empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				return true;
			}
		}
		return false;
	}
	size_t size() const { return current->size(); }
	void clear() { current->clear(); }
	auto begin() { return current->begin(); }
	auto end() { return current->end(); }
	const auto begin() const { return current->begin(); }
	const auto end() const { return current->end(); }
private:
	std::deque<T> buffers[2];
	std::deque<T>* current = buffers;
	std::deque<T>* next = buffers + 1;
	std::mutex mutex;
};
LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg == WM_CREATE) {
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTA*)lp)->lpCreateParams);
		return 0;
	}
	auto pWindow = (Window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
	if (pWindow) {
		auto& window = *pWindow;
		switch (msg) {
			case WM_SIZE: {
				if (LOWORD(lp) && HIWORD(lp)) {
					window.resize = true;
					window.clientSize = {
						LOWORD(lp),
						HIWORD(lp),
					};
				}
				return 0;
			}
			case WM_DESTROY: {
				window.running = false;
				return 0;
			}
			case WM_KILLFOCUS: {
				window.killFocus = true;
				return 0;
			}
			case WM_LBUTTONDOWN:
			case WM_SETFOCUS: {
				RECT rect;
				GetWindowRect(hwnd, &rect);
				rect.left += (rect.right - rect.left) / 2 - 1;
				rect.top += (rect.bottom - rect.top) / 2 - 1;
				rect.right = rect.left + 2;
				rect.bottom = rect.top + 2;
				ClipCursor(&rect);
				SetCursor(0);
				return 0;
			}
		}
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
using BlockID = u8;
struct Block {
	BlockID id = 0;
};
#define AXIS_PX 0
#define AXIS_NX 1
#define AXIS_PY 2
#define AXIS_NY 3
#define AXIS_PZ 4
#define AXIS_NZ 5
struct BlockInfo {
	enum class Type {
		default,
		x,
		topSideBottom,
	};
	u8 atlasPos = 0;
	Type type {};
	bool randomizeUv[6] {};
	u8 offsetAtlasPos(u8 axis) {
		switch (type) {
			case BlockInfo::Type::x:
			case BlockInfo::Type::default: 
				return atlasPos;
			case BlockInfo::Type::topSideBottom:
				if (axis == AXIS_PY)
					return atlasPos;
				if (axis == AXIS_NY)
					return atlasPos + 2;
				return atlasPos + 1;
			default:
				assert(0);
		}
	}
};
std::unordered_map<BlockID, BlockInfo> blockInfos;

#define BLOCK_AIR        ((BlockID)0)
#define BLOCK_DIRT       ((BlockID)1)
#define BLOCK_GRASS      ((BlockID)2)
#define BLOCK_TALL_GRASS ((BlockID)3)

bool isTransparent(BlockID id) {
	switch (id) {
		case BLOCK_AIR:
		case BLOCK_TALL_GRASS:
			return true;
	}
	return false;
}
bool isPhysical(BlockID id) {
	switch (id) {
		case BLOCK_AIR:
		case BLOCK_TALL_GRASS:
			return false;
	}
	return true;
}

#define CHUNK_SIZE 32
#define TOTAL_CHUNK_SIZE (CHUNK_SIZE *CHUNK_SIZE * CHUNK_SIZE)
#define FOR_BLOCK_IN_CHUNK 		   \
for (int z=0; z < CHUNK_SIZE; ++z) \
for (int x=0; x < CHUNK_SIZE; ++x) \
for (int y=0; y < CHUNK_SIZE; ++y) 
#define BLOCK_INDEX(x,y,z) ((z) * CHUNK_SIZE * CHUNK_SIZE + (x) * CHUNK_SIZE + (y))

#include "../data/shaders/blockVertex.h"

struct alignas(16) DrawCBuffer {
	static constexpr UINT slot = 0;
	M4 mvp;
	M4 model;
	V4 solidColor;
};
struct alignas(16) FrameCBuffer {
	static constexpr UINT slot = 1;
	V3 camPos;
};
struct alignas(16) SceneCBuffer {
	static constexpr UINT slot = 2;
	V4 fogColor;
	f32 fogDistance = 0;
};
struct alignas(16) BlitCBuffer {
	static constexpr UINT slot = 3;
	V2 sampleOffset;
};
struct Mesh {
	std::vector<Vertex> vertices;
	ID3D11Buffer* vBuffer = 0;
	ID3D11ShaderResourceView* vBufferView = 0;
	u32 vertexCount = 0;
	void load(Renderer& renderer, char* const path, std::vector<Vertex>&& verticesi) {
		vertices = std::move(verticesi);
		u32 vertexSize = sizeof(Vertex);
		vertexCount = (u32)vertices.size();
		assert(vertexCount);
		u32 vertexBufferSize = vertexCount * vertexSize;

		vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, vertexSize, vertices.data());

		vBufferView = renderer.createBufferView(vBuffer, vertexCount);
	}
	void draw(Renderer& renderer) {
		if (!vertexCount)
			return;
		renderer.deviceContext->VSSetShaderResources(0, 1, &vBufferView);
		renderer.deviceContext->Draw(vertexCount, 0);
	}
};
Mesh blockMesh;
#define SIZEOF_CHUNK (sizeof(Block) * TOTAL_CHUNK_SIZE)
#define SAVE_FILE DATA "save/world"
void printChunk(Block*, V3i);
std::mutex drawMutex;
V3i chunkPosFromBlock(V3 pos) {
	return {
		(int)floorf(pos.x / CHUNK_SIZE),
		(int)floorf(pos.y / CHUNK_SIZE),
		(int)floorf(pos.z / CHUNK_SIZE)
	};
}
V3i chunkPosFromBlock(V3i pos) {
	return chunkPosFromBlock((V3)pos);
}
V3 r2w(V3 rel, V3i chunk) {
	return rel + (V3)(chunk * CHUNK_SIZE);
}
V3i r2w(V3i rel, V3i chunk) {
	return rel + chunk * CHUNK_SIZE;
}
V3 w2r(V3 world, V3i chunk) {
	return world - (V3)(chunk * CHUNK_SIZE);
}
V3i w2r(V3i world, V3i chunk) {
	return world - chunk * CHUNK_SIZE;
}
std::atomic<size_t> vramUsage;
i64 generateTime, generateCount;
f32 generateMS;
std::mutex debugGenerateMutex;
struct Chunk;
#define CHUNK_PTR_NO_REF 1
#if CHUNK_PTR_NO_REF
using ChunkPtr = Chunk*;
#else
struct ChunkPtr {
	ChunkPtr() = default;
	ChunkPtr(int val) { assert(val == 0); }
	ChunkPtr(std::nullptr_t) {}
	ChunkPtr(Chunk* ptr) : ptr(ptr) {
		incref();
	}
	ChunkPtr(const ChunkPtr& other) {
		*this = other;
	}
	ChunkPtr(ChunkPtr&& other) {
		*this = std::move(other);
	}
	~ChunkPtr() {
		if (ptr) {
			decref();
		}
	}
	ChunkPtr& operator=(const ChunkPtr& other) {
		if (ptr) 
			decref();
		ptr = other.ptr;
		if (ptr)
			incref();
		return *this;
	}
	ChunkPtr& operator=(ChunkPtr&& other) {
		if (ptr)
			decref();
		ptr = other.ptr;
		if (ptr)
			incref();
		return *this;
	}
	Chunk* operator->() { return ptr; }
	const Chunk* operator->() const { return ptr; }
	operator bool() const { return ptr; }
	bool operator==(const ChunkPtr& other) const { return ptr == other.ptr; }
	bool operator!=(const ChunkPtr& other) const { return ptr != other.ptr; }
	Chunk* get() { return ptr; }
	void release();
private:
	Chunk* ptr = 0;
	friend struct std::hash<ChunkPtr>;
	void incref();
	void decref();
	u32 getref();
};
namespace std {
template<>
struct hash<ChunkPtr> {
	size_t operator()(const ChunkPtr& ptr) const {
		return std::hash<Chunk*>{}(ptr.ptr);
	}
};
}

#endif
struct Chunk {
	ID3D11Buffer* vBuffer = 0;
	//ID3D11Buffer* iBuffer = 0;
	ID3D11ShaderResourceView* vBufferView = 0;
	//ID3D11ShaderResourceView* iBufferView = 0;
	Block* blocks = 0;
	Renderer& renderer;
	V3i position;
	u32 vertexCount = 0;
	bool needToSave = false;
	bool generated = false;
	bool wantedToBeDeleted = false;
	bool meshGenerated = false;
	//bool cold = true;
	std::atomic_int userCount = 0;
	std::mutex deleteMutex;
#ifndef BUILD_RELEASE
	u32 refCount = 0;
	std::unordered_map<const char*, u32> users;
	std::set<ChunkPtr*> references;
#endif
	Chunk(V3i position, Renderer& renderer) : position(position), renderer(renderer) {}
	~Chunk() {
		assert(userCount == 0); // TODO: this fires sometimes
	}
	void allocateBlocks() {
		if (!blocks) {
			blocks = new Block[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE] {};
		}
	}
	void freeBlocks() {
		if (blocks) {
			delete blocks;
			blocks = 0;
		}
	}
	void free() {
		freeBlocks();
		if (vBuffer) vBuffer->Release();
		//if (iBuffer) iBuffer->Release();
		if (vBufferView) vBufferView->Release();
		vramUsage -= vertexCount * sizeof(Vertex);
		//if (iBufferView) iBufferView->Release();
	}
	FilePos filePos = FILEPOS_NOT_LOADED;
	bool load(FILE* chunkFile) {
		//TIMED_FUNCTION_;
		assert(chunkFile);

		filePos = BorisHash::find(position);
		if (filePos == FILEPOS_NOT_LOADED)
			return false;

		assert(filePos % SIZEOF_CHUNK == 0);
		_fseeki64(chunkFile, filePos, SEEK_SET);
		allocateBlocks();
		fread(blocks, SIZEOF_CHUNK, 1, chunkFile);
		generated = true;
		return true;
	}
	void save(FILE* chunkFile) {
		//TIMED_FUNCTION_;
		assert(chunkFile);
		if (filePos == FILEPOS_NOT_LOADED || !needToSave) {
			return;
		}
		assert(blocks);
		if (filePos == FILEPOS_APPEND) {
			_fseeki64(chunkFile, 0, SEEK_END);
			filePos = _ftelli64(chunkFile);
			assert(filePos % SIZEOF_CHUNK == 0);

			fwrite(blocks, SIZEOF_CHUNK, 1, chunkFile);

			BorisHash::add({position, filePos});
		}
		else {
			assert(filePos % SIZEOF_CHUNK == 0);
			_fseeki64(chunkFile, filePos, SEEK_SET);
			fwrite(blocks, SIZEOF_CHUNK, 1, chunkFile);
		}
	}
	// returns true if block is on boundary
	bool setBlock(int x, int y, int z, Block blk) {
		assert(x >= 0 && x < CHUNK_SIZE);
		assert(y >= 0 && y < CHUNK_SIZE);
		assert(z >= 0 && z < CHUNK_SIZE);
		auto& b = blocks[BLOCK_INDEX(x,y,z)];
		auto result = b.id != blk.id;
		b = blk;
		needToSave = true;
		return result;
	}
	bool setBlock(V3i p, Block blk) {
		return setBlock(p.x, p.y, p.z, blk);
	}
	Block& getBlock(int x, int y, int z) {
		assert(x >= 0 && x < CHUNK_SIZE);
		assert(y >= 0 && y < CHUNK_SIZE);
		assert(z >= 0 && z < CHUNK_SIZE);
		assert(blocks);
		return blocks[BLOCK_INDEX(x, y, z)];
	}
	Block& getBlock(V3i p) {
		return getBlock(p.x, p.y, p.z);
	}
	void generate() {
		//TIMED_FUNCTION_;
		filePos = FILEPOS_APPEND;
		allocateBlocks();
		if (generated)
			return;
		//printf("Generated ");
		//printChunk(blocks, position);
#if WORLD_SURFACE
		if (position.y < 0) {
			memset(blocks, 0x01010101, SIZEOF_CHUNK);
		}
		else {
			auto beginCounter = WH::getPerformanceCounter();
#if 0
			V2 rels[CHUNK_SIZE][CHUNK_SIZE];
			V2 tiles[CHUNK_SIZE][CHUNK_SIZE];
			for (int x = 0; x < CHUNK_SIZE; ++x) for (int z = 0; z < CHUNK_SIZE; ++z) {
				V2 seed = V2 {V2i{position.x, position.z} *CHUNK_SIZE + V2i{x, z}} / 256.0f / PI;
				tiles[x][z] = floor(seed);
				rels[x][z] = frac(seed) - 0.5f;
			}
			int results[CHUNK_SIZE][CHUNK_SIZE] {};
			for (int x = 0; x < CHUNK_SIZE; ++x) for (int z = 0; z < CHUNK_SIZE; ++z) {
				auto result = FLT_MAX;
				for (i32 sx = -1; sx <= 1; ++sx) {
					for (i32 sy = -1; sy <= 1; ++sy) {
						V2 off = {(f32)sx, (f32)sy};
						result = min(result, distanceSqr(rels[x][z], distanceSqr(rels[x][z], random01(tiles[x][z] + off) + off - 0.5f)));
					}
				}
				results[x][z] = (int)(sqrt(result) * (1/ROOT2) * 512 - position.y * CHUNK_SIZE);
			}

			for (int x = 0; x < CHUNK_SIZE; ++x) for (int z = 0; z < CHUNK_SIZE; ++z) {
				auto h = results[x][z];
				auto top = h;
				h = clamp(h, 0, CHUNK_SIZE);
				for (int y = 0; y < h; ++y) {
					blocks[BLOCK_INDEX(x, y, z)].id = y == top - 1 ? BLOCK_GRASS : BLOCK_DIRT;
				}
			}
#else
			for (int x = 0; x < CHUNK_SIZE; ++x) {
				for (int z = 0; z < CHUNK_SIZE; ++z) {
					V2 globalPos = V2 {(f32)(position.x * CHUNK_SIZE + x), (f32)(position.z * CHUNK_SIZE + z)};
					int h = 0;
					//h += (int)(128 - perlin(seed + 100000, 8) * 256);
					h += (int)(textureDetail(8, globalPos / 256.0f / PI, voronoi) * 512) + 2;
					h -= position.y * CHUNK_SIZE;
					auto top = h;
					h = clamp(h, 0, CHUNK_SIZE);
					for (int y = 0; y < h; ++y) {
						auto b = BLOCK_DIRT;
						if (y == top - 1) {
							//b = BLOCK_TALL_GRASS;
							b = perlin(globalPos / PI, 2) > 0.5f ? BLOCK_TALL_GRASS : BLOCK_AIR;
						}
						if (y == top - 2) b = BLOCK_GRASS;
						blocks[BLOCK_INDEX(x, y, z)].id = b;
					}
				}
			}
#endif
			auto endCounter = WH::getPerformanceCounter();
			debugGenerateMutex.lock();
			generateTime += endCounter - beginCounter;
			++generateCount;
			if (generateCount == 10000) {
				generateMS = (f32)(generateTime / 10000) / WH::getPerformanceFrequency() * 1000.f;
				generateTime = 0;
				generateCount = 0;
			}
			debugGenerateMutex.unlock();
		}
#else
		FOR_BLOCK_IN_CHUNK {
			if (voronoi((V3(position * CHUNK_SIZE) + V3((f32)x, (f32)y, (f32)z)) / 64.0f * PI) < 0.1f)
				blocks[BLOCK_INDEX(x, y, z)].id = BLOCK_GRASS;
		}
#endif
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		generated = true;
	}
	void generateMesh(const std::array<ChunkPtr, 6>& neighbors) {
		deleteMutex.lock();
		if (wantedToBeDeleted) {
			deleteMutex.unlock();
			return;
		}
		deleteMutex.unlock();
		//TIMED_FUNCTION;
		assert(blocks);
#define RESERVE_COUNT 4
		std::vector<Vertex> vertices;
		vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 4 * RESERVE_COUNT);
		std::vector<u32> indices;
		indices.reserve(CHUNK_SIZE * CHUNK_SIZE * 6 * RESERVE_COUNT);
#undef RESERVE_COUNT
		//u32 idxOff = 0;
		FOR_BLOCK_IN_CHUNK {
			auto & b = getBlock(x,y,z);
			switch (b.id) {
				case 0:
					continue;
			}
			auto& blockInfo = blockInfos.at(b.id);

			switch (blockInfo.type) {
				case BlockInfo::Type::default:
				case BlockInfo::Type::topSideBottom: {
					bool visible[6];
					visible[0] = x == CHUNK_SIZE - 1 ? (neighbors[0] ? isTransparent(neighbors[0]->getBlock             (0, y, z).id) : true) : isTransparent(getBlock(x + 1, y, z).id);
					visible[1] =              x == 0 ? (neighbors[1] ? isTransparent(neighbors[1]->getBlock(CHUNK_SIZE - 1, y, z).id) : true) : isTransparent(getBlock(x - 1, y, z).id);
					visible[2] = y == CHUNK_SIZE - 1 ? (neighbors[2] ? isTransparent(neighbors[2]->getBlock             (x, 0, z).id) : true) : isTransparent(getBlock(x, y + 1, z).id);
					visible[3] =              y == 0 ? (neighbors[3] ? isTransparent(neighbors[3]->getBlock(x, CHUNK_SIZE - 1, z).id) : true) : isTransparent(getBlock(x, y - 1, z).id);
					visible[4] = z == CHUNK_SIZE - 1 ? (neighbors[4] ? isTransparent(neighbors[4]->getBlock             (x, y, 0).id) : true) : isTransparent(getBlock(x, y, z + 1).id);
					visible[5] =              z == 0 ? (neighbors[5] ? isTransparent(neighbors[5]->getBlock(x, y, CHUNK_SIZE - 1).id) : true) : isTransparent(getBlock(x, y, z - 1).id);
					auto calcVerts = [&](u8 axis) {
						Vertex verts[4] {};
						switch (axis) {
							case AXIS_PX:
								verts[0].setPosition(x, y, z, 1, axis);
								verts[1].setPosition(x, y, z, 0, axis);
								verts[2].setPosition(x, y, z, 3, axis);
								verts[3].setPosition(x, y, z, 2, axis);
								break;
							case AXIS_NX:
								verts[0].setPosition(x, y, z, 4, axis);
								verts[1].setPosition(x, y, z, 5, axis);
								verts[2].setPosition(x, y, z, 6, axis);
								verts[3].setPosition(x, y, z, 7, axis);
								break;
							case AXIS_PY:
								verts[0].setPosition(x, y, z, 4, axis);
								verts[1].setPosition(x, y, z, 0, axis);
								verts[2].setPosition(x, y, z, 5, axis);
								verts[3].setPosition(x, y, z, 1, axis);
								break;
							case AXIS_NY:
								verts[0].setPosition(x, y, z, 7, axis);
								verts[1].setPosition(x, y, z, 3, axis);
								verts[2].setPosition(x, y, z, 6, axis);
								verts[3].setPosition(x, y, z, 2, axis);
								break;
							case AXIS_PZ:
								verts[0].setPosition(x, y, z, 0, axis);
								verts[1].setPosition(x, y, z, 4, axis);
								verts[2].setPosition(x, y, z, 2, axis);
								verts[3].setPosition(x, y, z, 6, axis);
								break;
							case AXIS_NZ:
								verts[0].setPosition(x, y, z, 5, axis);
								verts[1].setPosition(x, y, z, 1, axis);
								verts[2].setPosition(x, y, z, 7, axis);
								verts[3].setPosition(x, y, z, 3, axis);
								break;
							default:
								assert(0);
						}
						int pos = blockInfo.offsetAtlasPos(axis);
						V2i atlasPos {
							pos % ATLAS_SIZE,
							pos / ATLAS_SIZE,
						};
						u32 uvIdx = 0;
						if (blockInfo.randomizeUv[axis]) {
							uvIdx = randomU32(r2w(V3i {x,y,z}, position)) % 4;
						}
						V2i uvs[4];
						uvs[0] = {atlasPos.x,  atlasPos.y};
						uvs[1] = {atlasPos.x + 1,atlasPos.y};
						uvs[2] = {atlasPos.x + 1,atlasPos.y + 1};
						uvs[3] = {atlasPos.x,  atlasPos.y + 1};
						verts[0].setUv(uvs[(uvIdx + 0) % 4]);
						verts[1].setUv(uvs[(uvIdx + 1) % 4]);
						verts[2].setUv(uvs[(uvIdx + 3) % 4]);
						verts[3].setUv(uvs[(uvIdx + 2) % 4]);
						vertices.push_back(verts[0]);
						vertices.push_back(verts[1]);
						vertices.push_back(verts[2]);
						vertices.push_back(verts[1]);
						vertices.push_back(verts[3]);
						vertices.push_back(verts[2]);
					};
					if (visible[0]) calcVerts(AXIS_PX);
					if (visible[1]) calcVerts(AXIS_NX);
					if (visible[2]) calcVerts(AXIS_PY);
					if (visible[3]) calcVerts(AXIS_NY);
					if (visible[4]) calcVerts(AXIS_PZ);
					if (visible[5]) calcVerts(AXIS_NZ);
					break;
				}
				case BlockInfo::Type::x: {
					Vertex verts[4] {};
					V2i atlasPos {
						 blockInfo.atlasPos % ATLAS_SIZE,
						 blockInfo.atlasPos / ATLAS_SIZE,
					};
					V2i uvs[4];
					uvs[0] = {atlasPos.x,  atlasPos.y};
					uvs[1] = {atlasPos.x + 1,atlasPos.y};
					uvs[2] = {atlasPos.x + 1,atlasPos.y + 1};
					uvs[3] = {atlasPos.x,  atlasPos.y + 1};
					auto randomizeUv = [&](u32 o) {
						if (randomU32(r2w(V3i {x,y,z} + o, position)) & 1) {
							verts[0].setUv(uvs[0]);
							verts[1].setUv(uvs[1]);
							verts[2].setUv(uvs[3]);
							verts[3].setUv(uvs[2]);
						}
						else {
							verts[0].setUv(uvs[1]);
							verts[1].setUv(uvs[0]);
							verts[2].setUv(uvs[2]);
							verts[3].setUv(uvs[3]);
						}
					};
					auto insertVertices = [&](u32 p, u32 a) {
						randomizeUv((p << 1) | a);
						verts[0].data0 = makeVertexData0(x, y, z, 0 + p, AXIS_PY);
						verts[1].data0 = makeVertexData0(x, y, z, 5 - p, AXIS_PY);
						verts[2].data0 = makeVertexData0(x, y, z, 2 + p, AXIS_PY);
						verts[3].data0 = makeVertexData0(x, y, z, 7 - p, AXIS_PY);
						vertices.push_back(verts[0]);
						vertices.push_back(verts[a ? 1 : 2]);
						vertices.push_back(verts[a ? 2 : 1]);
						vertices.push_back(verts[1]);
						vertices.push_back(verts[a ? 3 : 2]);
						vertices.push_back(verts[a ? 2 : 3]);
					};
					insertVertices(0, 0);
					insertVertices(0, 1);
					insertVertices(1, 0);
					insertVertices(1, 1);
					break;
				}
				default:
					assert(0);
			}
		}

		u32 vertexSize = sizeof(Vertex);
		//u32 indexSize = sizeof(u32);
		//u32 indexCount = (u32)indices.size();
		if (vertexCount) {
			vramUsage -= vertexCount * sizeof(Vertex);
		}
		vertexCount = (u32)vertices.size();
		//u32 indexBufferSize = indexCount * indexSize;
		u32 vertexBufferSize = vertexCount * vertexSize;

		std::unique_lock l(drawMutex);
		if (vBuffer) {
			vBuffer->Release();
			vBuffer = 0;
		}
		if (vBufferView) {
			vBufferView->Release();
			vBufferView = 0;
		}
		if (!vertexCount)
			return;

		//if (iBuffer) iBuffer->Release();
		//if (iBufferView) iBufferView->Release();

		vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, vertexSize, vertices.data());
		meshGenerated = true;
		vramUsage += vertexBufferSize;
		//iBuffer = renderer.createImmutableStructuredBuffer(indexBufferSize, indexSize, indices.data());

		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = vertexCount;
		DHR(renderer.device->CreateShaderResourceView(vBuffer, &desc, &vBufferView));
		//desc.Buffer.NumElements = indexCount;
		//DHR(renderer.device->CreateShaderResourceView(iBuffer, &desc, &iBufferView));
	}
	void draw(V3i playerChunk, const M4& matrixVP, ID3D11Buffer* cBuffer) const {
		if (!vertexCount || !vBufferView)
			return;

		DrawCBuffer data;
		data.model = M4::translation((V3)((position - playerChunk) * CHUNK_SIZE));
		data.mvp = matrixVP * data.model;
		data.solidColor = 1;
		renderer.updateBuffer(cBuffer, data);
		drawMutex.lock();
		renderer.deviceContext->VSSetShaderResources(0, 1, &vBufferView);
		//renderer.deviceContext->VSSetShaderResources(1, 1, &iBufferView);
		drawMutex.unlock();
		renderer.deviceContext->Draw(vertexCount, 0);
	}
};
#if !CHUNK_PTR_NO_REF
u32 ChunkPtr::getref() {
	return ptr->refCount;
}
inline void ChunkPtr::release() {
	decref();
	delete ptr;
	ptr = 0;
}
void ChunkPtr::incref() {
	++ptr->refCount;
	ptr->references.insert(this);
}
void ChunkPtr::decref() {
	assert(ptr->refCount);
	--ptr->refCount;
	ptr->references.erase(this);
}
#endif
/*
void printChunk(Block* blocks, V3i pos) {
	auto usedBlocks = 0;
	for (int j = 0; j < TOTAL_CHUNK_SIZE; ++j) {
		if (blocks[j].id == BlockID::cube) ++usedBlocks;
	}
	printf("chunk %i %i %i, used blocks: %u\n", pos.x, pos.y, pos.z, usedBlocks);
}
*/
#if 0
#define ARENA_CAPACITY 0x10000
struct ChunkArena {
	struct alignas(64) _Entry { Chunk c; bool b; };
	struct Block {
		bool used[ARENA_CAPACITY] {};
		Chunk memory[ARENA_CAPACITY];
		Block* next = 0;
	};
	Block* start = 0;
	Block* current = 0;
	ChunkArena() {
		start = new Block;
		current = start;
	}
	ChunkPtr allocate() {
		for (u32 i = 0; i < capacity; ++i) {
			if (!used[i]) {
				used[i] = true;
				return new(memory + i) Chunk;
			}
		}
		assert(!"Not enough memory");
	}
	void free(ChunkPtr ptr) {
		u32 i = ptr - memory;
		assert(used[i]);
		used[i] = false;
		ptr->~Chunk();
	}
};
#endif
int maxDistance(V3i a, V3i b) {
	a = (a-b).absolute();
	return max(max(a.x, a.y), a.z);
}
#ifdef BUILD_RELEASE
#define HOT_DIST 4
#else
#define HOT_DIST 1
#endif
struct World {
	std::unordered_map<V3i, ChunkPtr> loadedChunks;
	DoubleQueue<ChunkPtr> loadQueue, unloadQueue, meshQueue; // , coldQueue;
	std::atomic_bool loadQueueNoPush = false, meshQueueNoPush = false;
	//std::unordered_set<ChunkPtr> visibleChunks;
	std::vector<ChunkPtr> chunksWantedToDelete;
	Renderer& renderer;
	const V3i& playerChunk;
	const int drawDistance;
	const int hotDistance;
	World(Renderer& renderer, const V3i& playerChunk, int drawDistance) : renderer(renderer), playerChunk(playerChunk), drawDistance(drawDistance), hotDistance(min(drawDistance, HOT_DIST)) {
		int w = drawDistance * 2 + 1;
		loadedChunks.reserve(w * w * w);
		BorisHash::init();
		//BorisHash::debug();
	}
	void seeChunk(V3i pos) {
		getChunkUnchecked(pos);
		//visibleChunks.emplace(getChunkUnchecked(pos));
	}
	// main thread, called when player moves to other chunk
	void unseeChunks() {
		//std::vector<ChunkPtr> toUnsee;
		//toUnsee.reserve(drawDistance * 2 + 1);
		//for (auto& c : visibleChunks) {
		//	if (maxDistance(playerChunk, c->position) > drawDistance) {
		//		toUnsee.push_back(c);
		//	}
		//}
		//for (auto& c : toUnsee)
		//	visibleChunks.erase(c);

		std::vector<ChunkPtr> chunksToUnload;
		chunksToUnload.reserve(drawDistance * 2 + 1);
		for (auto& [pos, c] : loadedChunks) {
			auto dist = maxDistance(playerChunk, c->position);
			//c->cold = dist > hotDistance;
			if (dist > drawDistance) {
				unloadQueue.push(c);
				chunksToUnload.push_back(c);
			}
		}
		for (auto& c : chunksToUnload) {
			loadedChunks.erase(c->position);
		}
	}
	// main thread
	void save() {
		auto chunkFile = openFileRW(SAVE_FILE);
		size_t progress = 0;
		for (auto& [position, chunk] : loadedChunks) {
			chunk->save(chunkFile);
			chunk->free();
#if CHUNK_PTR_NO_REF
			delete chunk;
#else
			chunk.release();
#endif
			printf("Saving world... %zu%%\r", progress++ * 100 / loadedChunks.size());
		}
		puts("Saving world... 100%");
		fclose(chunkFile);
		BorisHash::shutdown();
	}
	std::array<ChunkPtr, 6> getNeighbors(ChunkPtr c) {
		std::array<ChunkPtr, 6> neighbors;
		neighbors[0] = findChunk(c->position + V3i { 1, 0, 0});
		neighbors[1] = findChunk(c->position + V3i {-1, 0, 0});
		neighbors[2] = findChunk(c->position + V3i { 0, 1, 0});
		neighbors[3] = findChunk(c->position + V3i { 0,-1, 0});
		neighbors[4] = findChunk(c->position + V3i { 0, 0, 1});
		neighbors[5] = findChunk(c->position + V3i { 0, 0,-1});
		for (int i=0; i < 6;++i) {
			if (neighbors[i] && !neighbors[i]->blocks)
				neighbors[i] = 0;
		}
		return neighbors;
	}
	// chunk loader thread
	void wantToDelete(ChunkPtr c) {
		if (!c->wantedToBeDeleted) {
			c->wantedToBeDeleted = true;
			chunksWantedToDelete.push_back(c);
		}
	}
	// chunk loader thread
	bool loadChunks() {
		auto file = fopen(SAVE_FILE, "rb");
		DEFER { if(file) fclose(file);};

		auto processQueue = [&]() {
			auto loadMesh = [&](ChunkPtr c) {
				++c->userCount;
#ifndef BUILD_RELEASE
				++c->users["meshQueue"];
#endif
				meshQueue.push(c);
			};
			printf("To load: %zu\n", loadQueue.size());
			for (auto& c : loadQueue) {
				DEFER {
					--c->userCount;
#ifndef BUILD_RELEASE
					--c->users["loadQueue"];
#endif
				};
				if (maxDistance(playerChunk, c->position) > drawDistance) {
					wantToDelete(c);
				}
				else {
					c->wantedToBeDeleted = false;
				}
				if (loadQueueNoPush)
					continue;
				if (!file || !c->load(file)) {
					c->generate();
				}
				loadMesh(c);
				for (auto& n : getNeighbors(c)) {
					if (n) {
						loadMesh(n);
					}
				}
			}
			loadQueue.clear();
			loadQueue.swap();
		};

		if (loadQueueNoPush) {
			processQueue();
			processQueue();
			meshQueueNoPush = true;
			return false;
		}
		if (loadQueue.empty()) 
			return true;
		processQueue();
		return true;
	}
	void unloadChunks() {
		if (unloadQueue.empty()) 
			return;
		printf("To save: %zu\n", unloadQueue.size());
		auto file = openFileRW(SAVE_FILE);
		for (auto& c : unloadQueue) {
			wantToDelete(c);
		}
		fclose(file);
		unloadQueue.clear();
		unloadQueue.swap();

		std::vector<ChunkPtr> toDelete;
		std::vector<ChunkPtr> notToDelete;
		for (auto& c : chunksWantedToDelete) {
			if (!c->wantedToBeDeleted)
				continue;
			c->deleteMutex.lock();
			auto canDelete = c->userCount == 0;
			c->deleteMutex.unlock();
			if(canDelete)
				toDelete.push_back(c);
			else
				notToDelete.push_back(c);
		}
		chunksWantedToDelete = std::move(notToDelete);
		printf("To delete: %zu\n", toDelete.size());
		for (auto& c : toDelete) {
			c->save(file);
			c->free();
#if CHUNK_PTR_NO_REF
			delete c;
#else
			if (c->refCount != 1) {
				for (const auto& r : c->references) {
					for (const auto& t : meshQueue) if (*r == t) puts("meshQueue");
					for (const auto& t : loadQueue) if (*r == t) puts("loadQueue");
					for (const auto& t : unloadQueue) if (*r == t) puts("unloadQueue");
					for (const auto& t : loadedChunks) if (*r == t.second) puts("loadedChunks");
					for (const auto& t : visibleChunks) if (*r == t) puts("visibleChunks");
				}
				assert(0);
				}
			c.release();
#endif
		}
	}
	/*
	void freeColdChunks() {
		auto file = openFileRW(SAVE_FILE);
		DEFER {fclose(file);};
		auto processQueue = [&]() {
			for (auto& c : coldQueue) {
				DEFER {
					--c->userCount;
#ifndef BUILD_RELEASE
					--c->users["coldQueue"];
#endif
				};
				//if (coldQueueNoPush)
				//	continue;
				if (c->meshGenerated) {
					c->save(file);
					c->freeBlocks();
				}
			}
			coldQueue.clear();
			coldQueue.swap();
		};
		//if (coldQueueNoPush) {
		//	processQueue();
		//	processQueue();
		//	return false;
		//}
		if (coldQueue.empty())
			return;
		processQueue();

	}
	*/
	bool updateChunks() {
		unloadChunks();
		//freeColdChunks();
		return loadChunks();
	}
	bool generateMeshes() {
		auto processQueue = [&]() {
			//printf("Meshes to generate: %zu\n", meshQueue.size());
			for (auto& c : meshQueue) {
				DEFER {
					--c->userCount;
#ifndef BUILD_RELEASE
					--c->users["meshQueue"];
#endif
				};
				if (meshQueueNoPush)
					continue;
				c->generateMesh(getNeighbors(c));
				/*
				if (c->cold) {
					++c->userCount;
#ifndef BUILD_RELEASE
					++c->users["coldQueue"];
#endif
					coldQueue.push(c);
				}
				*/
			}
			meshQueue.clear();
			meshQueue.swap();
		};
		if (meshQueueNoPush) {
			processQueue();
			processQueue();
			return false;
		}
		if (meshQueue.empty())
			return true;
		processQueue();
		return true;
	}
	ChunkPtr findChunk(V3i pos) {
		if (auto it = loadedChunks.find(pos); it != loadedChunks.end())
			return it->second;
		return 0;
	}
	ChunkPtr getChunkUnchecked(V3i pos) {
		ChunkPtr chunk = findChunk(pos);
		if (chunk)
			return chunk;
		chunk = new Chunk(pos, renderer);
		loadedChunks[pos] = chunk;
		++chunk->userCount;
#ifndef BUILD_RELEASE
		++chunk->users["loadQueue"];
#endif
		loadQueue.push(chunk);
		return chunk;
	}
	ChunkPtr getChunk(V3i pos) {
		auto c = getChunkUnchecked(pos);
		if (c->generated)
			return c;
		return 0;
	}
	ChunkPtr getChunkFromBlock(V3i pos) {
		return getChunk(chunkPosFromBlock(pos));
	}
	bool setBlock(V3i worldPos, Block blk, bool immediate = false) {
		auto chunkPos = chunkPosFromBlock(worldPos);
		auto c = getChunk(chunkPos);
		if (!c)
			return false;
		auto relPos = frac(worldPos, CHUNK_SIZE);
		if (c->setBlock(relPos, blk)) {
			auto updateMesh = [this, immediate](ChunkPtr chunk) {
				if (immediate) {
					chunk->generateMesh(getNeighbors(chunk));
				}
				else
					meshQueue.push(chunk);
			};
			updateMesh(c);
			auto onBoundary = [](V3i p) {
				return
					p.x == 0 || p.x == CHUNK_SIZE - 1 ||
					p.y == 0 || p.y == CHUNK_SIZE - 1 ||
					p.z == 0 || p.z == CHUNK_SIZE - 1;
			};
			if (relPos.x == 0) if (auto n = getChunk(chunkPos + V3i {-1,0,0}); n)  updateMesh(n);
			if (relPos.y == 0) if (auto n = getChunk(chunkPos + V3i {0,-1,0}); n)  updateMesh(n);
			if (relPos.z == 0) if (auto n = getChunk(chunkPos + V3i {0,0,-1}); n)  updateMesh(n);
			if (relPos.x == CHUNK_SIZE - 1) if (auto n = getChunk(chunkPos + V3i {1,0,0}); n)  updateMesh(n);
			if (relPos.y == CHUNK_SIZE - 1) if (auto n = getChunk(chunkPos + V3i {0,1,0}); n)  updateMesh(n);
			if (relPos.z == CHUNK_SIZE - 1) if (auto n = getChunk(chunkPos + V3i {0,0,1}); n)  updateMesh(n);
			return true;
		}
		return false;
	}
	bool setBlock(V3 pos, Block blk, bool immediate = false) {
		return setBlock((V3i)pos, blk);
	}
	std::optional<Block> getBlock(V3i pos) {
		if (auto c = getChunkFromBlock(pos); c)
			return c->getBlock(frac(pos, CHUNK_SIZE));
		return {};
	}
	bool canWalkInto(V3i pos) {
		if (auto c = getChunkFromBlock(pos); c)
			return !isPhysical(c->getBlock(frac(pos, CHUNK_SIZE)).id);
		return false;
	}
};
struct Position {
	void setWorld(V3 pos) {
		relPos = pos;
		chunkPos = {};
		normalize();
	}
	V3 getWorld() {
		return V3 {chunkPos * CHUNK_SIZE} +relPos;
	}
	bool move(V3 delta) {
		relPos += delta;
		return normalize();
	}
	bool normalize() {
		auto oldChunk = chunkPos;
		bool chunkChanged = false;
		if (relPos.x >= CHUNK_SIZE) { chunkPos.x += int(relPos.x / CHUNK_SIZE); relPos.x = fmodf(relPos.x, CHUNK_SIZE); chunkChanged = true; }
		if (relPos.y >= CHUNK_SIZE) { chunkPos.y += int(relPos.y / CHUNK_SIZE); relPos.y = fmodf(relPos.y, CHUNK_SIZE); chunkChanged = true; }
		if (relPos.z >= CHUNK_SIZE) { chunkPos.z += int(relPos.z / CHUNK_SIZE); relPos.z = fmodf(relPos.z, CHUNK_SIZE); chunkChanged = true; }
		if (relPos.x < 0) { relPos.x = fabsf(relPos.x); chunkPos.x -= int(relPos.x / CHUNK_SIZE) + 1; relPos.x = -fmodf(relPos.x, CHUNK_SIZE) + CHUNK_SIZE; chunkChanged = true; }
		if (relPos.y < 0) { relPos.y = fabsf(relPos.y); chunkPos.y -= int(relPos.y / CHUNK_SIZE) + 1; relPos.y = -fmodf(relPos.y, CHUNK_SIZE) + CHUNK_SIZE; chunkChanged = true; }
		if (relPos.z < 0) { relPos.z = fabsf(relPos.z); chunkPos.z -= int(relPos.z / CHUNK_SIZE) + 1; relPos.z = -fmodf(relPos.z, CHUNK_SIZE) + CHUNK_SIZE; chunkChanged = true; }
		return chunkChanged;
	}
	V3 relPos;
	V3i chunkPos;
};
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	/*
	for (u32 i=0; i < 0x10; ++i) {
		//printf("%u\n", randomU32(i));
		printf("%u\n", randomU32(V3i {(int)i,(int)i,(int)i}));
	}
	for (u32 i=0xFFFFFFFF; i > 0xFFFFFFEF; --i) {
		//printf("%u\n", randomU32(i));
		printf("%u\n", randomU32(V3i {(int)i,(int)i,(int)i}));
	}
	*/

	//SetConsoleCtrlHandler([](DWORD type) -> BOOL {
	//	if (type == CTRL_CLOSE_EVENT) {
	//		assert(0);
	//	}
	//	return 1;
	//}, 1);

	const f32 maxSpeed = 15;
	const f32 jumpForce = 250;
	const f32 airMult = 0.2;
	const f32 noclipMult = 16;
	const f32 groundFriction = 1;
	const f32 airFriction = 0.2f;
	const f32 noclipFriction = 1;
	const f32 noclipMaxSpeed = 50;
	const f32 camHeight = 0.625f;

	Position spawnPos;
	spawnPos.setWorld({0, 250, 0});

	Position playerPos = spawnPos;
	V3 playerVel;
	V3 playerDimH = {0.375f, 0.875f, 0.375f};
	V3 cameraRot;

#ifdef BUILD_RELEASE
#define DEFAULT_DRAW_DISTANCE 8
#else
#define DEFAULT_DRAW_DISTANCE 2
#endif

	int chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
	int superSampleScale = 1;

	puts(R"(		Меню
	1 - Начать
	2 - Настроить)");

	char menuItem;
	std::cin >> menuItem;
	if (menuItem == '2') {
		puts("Дистанция прорисовки (рекомендую от 4 до 16)");
		std::cin >> chunkDrawDistance;
		if (chunkDrawDistance <= 0) {
			chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
			puts("так низя. ставлю " STRINGIZE(DEFAULT_DRAW_DISTANCE));
		}
		MEMORYSTATUSEX memoryStatus {};
		memoryStatus.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memoryStatus);
		size_t x = chunkDrawDistance * 2 + 1;
		x *= CHUNK_SIZE * sizeof(Block);
		size_t memoryNeeded = x * x * x;
		if (memoryNeeded > memoryStatus.ullTotalPhys) {
			auto [value,unit] = normalizeBytes<double>(memoryNeeded);
			printf("Не хватает памяти (%.3f %s необходимо). Ставлю " STRINGIZE(DEFAULT_DRAW_DISTANCE) "\n", value, unit);
			chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
		}
		puts("Сглаживание [1,4]\n\t1 - Нет\n\t2 - 4x\n\t3 - 9x\n\t4 - 16x");
		std::cin >> superSampleScale;
		if (superSampleScale < 1 || superSampleScale > 4) {
			superSampleScale = 1;
			puts("так низя. ставлю 1");
		}
	}

	WH::WndClassExA wndClass;
	wndClass.hInstance = instance;
	wndClass.lpfnWndProc = wndProc;
	wndClass.lpszClassName = "gstorm";
	wndClass.checkIn();

	auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
	auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

	Window window;
	window.clientSize = {1280,720};
	WH::Rect windowRect;
	windowRect.xywh((screenWidth - (LONG)window.clientSize.x) / 2,
		(screenHeight - (LONG)window.clientSize.y) / 2,
					(LONG)window.clientSize.x,
					(LONG)window.clientSize.y);

	AdjustWindowRect(&windowRect, WINDOW_STYLE, 0);

	window.hwnd = CreateWindowExA(0, wndClass.lpszClassName, "Galaxy Storm", WINDOW_STYLE,
								windowRect.left, windowRect.top, windowRect.width(), windowRect.height(), 0, 0, instance, &window);
	assert(window.hwnd);

	RAWINPUTDEVICE RawInputMouseDevice = {};
	RawInputMouseDevice.usUsagePage = 0x01;
	RawInputMouseDevice.usUsage = 0x02;

	if (!RegisterRawInputDevices(&RawInputMouseDevice, 1, sizeof(RAWINPUTDEVICE))) {
		assert(0);
	}

	Renderer renderer(window.hwnd, window.clientSize * superSampleScale);
	renderer.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11RenderTargetView* backBuffer = 0;
	ID3D11DepthStencilView* depthView = 0;
	ID3D11Texture2D* depthTex = 0;

	ID3D11VertexShader* blockVS = renderer.createVertexShader(LDATA "shaders/block.hlsl");
	ID3D11PixelShader* blockPS  = renderer.createPixelShader (LDATA "shaders/block.hlsl");

	ID3D11VertexShader* blitVS = 0;
	ID3D11PixelShader*  blitPS = 0;
	switch (superSampleScale) {
		case 1: 
			break;
		case 2:
			blitVS = renderer.createVertexShader(LDATA "shaders/blit2x2.hlsl");
			blitPS = renderer.createPixelShader (LDATA "shaders/blit2x2.hlsl");
			break;
		case 3:
			blitVS = renderer.createVertexShader(LDATA "shaders/blit3x3.hlsl");
			blitPS = renderer.createPixelShader (LDATA "shaders/blit3x3.hlsl");
			break;
		case 4:
			blitVS = renderer.createVertexShader(LDATA "shaders/blit4x4.hlsl");
			blitPS = renderer.createPixelShader (LDATA "shaders/blit4x4.hlsl");
			break;
		default:
			assert(0);
	}

	f32 farClipPlane = 1000;
	V4 clearColor{0.4f, 0.7f, 1, 1};

	DrawCBuffer  drawCBufferData;
	FrameCBuffer frameCBufferData;
	SceneCBuffer sceneCBufferData;
	BlitCBuffer  blitCBufferData;

	sceneCBufferData.fogColor = clearColor;
	sceneCBufferData.fogDistance = (f32)chunkDrawDistance * CHUNK_SIZE;

	ID3D11Buffer* drawCBuffer  = renderer.createDynamicBuffer  (D3D11_BIND_CONSTANT_BUFFER, sizeof(drawCBufferData ), 0, 0);
	ID3D11Buffer* frameCBuffer = renderer.createDynamicBuffer  (D3D11_BIND_CONSTANT_BUFFER, sizeof(frameCBufferData), 0, 0);
	ID3D11Buffer* sceneCBuffer = renderer.createImmutableBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(sceneCBufferData), 0, 0, &sceneCBufferData);
	ID3D11Buffer* blitCBuffer  = renderer.createDynamicBuffer  (D3D11_BIND_CONSTANT_BUFFER, sizeof(blitCBufferData ), 0, 0);

	renderer.deviceContext->VSSetConstantBuffers(DrawCBuffer ::slot, 1, &drawCBuffer);
	renderer.deviceContext->PSSetConstantBuffers(DrawCBuffer ::slot, 1, &drawCBuffer);
	renderer.deviceContext->VSSetConstantBuffers(FrameCBuffer::slot, 1, &frameCBuffer);
	renderer.deviceContext->PSSetConstantBuffers(FrameCBuffer::slot, 1, &frameCBuffer);
	renderer.deviceContext->VSSetConstantBuffers(SceneCBuffer::slot, 1, &sceneCBuffer);
	renderer.deviceContext->PSSetConstantBuffers(SceneCBuffer::slot, 1, &sceneCBuffer);
	renderer.deviceContext->VSSetConstantBuffers(BlitCBuffer ::slot, 1, &blitCBuffer);
	renderer.deviceContext->PSSetConstantBuffers(BlitCBuffer ::slot, 1, &blitCBuffer);

	M4 projection;

	blockInfos[BLOCK_DIRT] = {2, BlockInfo::Type::default, {1,1,1,1,1,1}};
	blockInfos[BLOCK_GRASS] = {0, BlockInfo::Type::topSideBottom, {0,0,1,1,0,0}};
	blockInfos[BLOCK_TALL_GRASS] = {3, BlockInfo::Type::x};

	auto vertsFromFaces = [](const std::vector<Vertex>& verts) {
		std::vector<Vertex> result;
		assert(verts.size() % 4 == 0);
		result.reserve(verts.size() / 2 * 3);
		for (int i=0; i < verts.size(); i+=4) {
			result.push_back(verts[i + 0]);
			result.push_back(verts[i + 1]);
			result.push_back(verts[i + 2]);
			result.push_back(verts[i + 1]);
			result.push_back(verts[i + 3]);
			result.push_back(verts[i + 2]);
		}
		return result;
	};

#define M 32
	blockMesh.load(renderer, DATA "mesh/block.mesh", 
				   vertsFromFaces({{makeVertexData0(0,0,0,5, AXIS_NZ), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,1, AXIS_NZ), makeVertexData1(M,M)},// -z
				   				   {makeVertexData0(0,0,0,7, AXIS_NZ), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,3, AXIS_NZ), makeVertexData1(M,0)},
				   				   {makeVertexData0(0,0,0,0, AXIS_PZ), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,4, AXIS_PZ), makeVertexData1(M,M)},// +z
				   				   {makeVertexData0(0,0,0,2, AXIS_PZ), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,6, AXIS_PZ), makeVertexData1(M,0)},
				   				   {makeVertexData0(0,0,0,7, AXIS_NY), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,3, AXIS_NY), makeVertexData1(M,M)},// -y
				   				   {makeVertexData0(0,0,0,6, AXIS_NY), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,2, AXIS_NY), makeVertexData1(M,0)},
				   				   {makeVertexData0(0,0,0,4, AXIS_PY), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,0, AXIS_PY), makeVertexData1(M,M)},// +y
				   				   {makeVertexData0(0,0,0,5, AXIS_PY), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,1, AXIS_PY), makeVertexData1(M,0)},
				   				   {makeVertexData0(0,0,0,4, AXIS_NX), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,5, AXIS_NX), makeVertexData1(M,M)},// -x
				   				   {makeVertexData0(0,0,0,6, AXIS_NX), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,7, AXIS_NX), makeVertexData1(M,0)},
				   				   {makeVertexData0(0,0,0,1, AXIS_PX), makeVertexData1(0,M)}, {makeVertexData0(0,0,0,0, AXIS_PX), makeVertexData1(M,M)},// +x
				   				   {makeVertexData0(0,0,0,3, AXIS_PX), makeVertexData1(0,0)}, {makeVertexData0(0,0,0,2, AXIS_PX), makeVertexData1(M,0)},
	}));
#undef M
	ID3D11ShaderResourceView* atlasTex     = renderer.createTexture(DATA "textures/atlas.png");
	ID3D11ShaderResourceView* selectionTex = renderer.createTexture(DATA "textures/selection.png");

	ID3D11SamplerState* samplerState = renderer.createSamplerState(D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, 3);
	renderer.deviceContext->PSSetSamplers(0, 1, &samplerState);

	ID3D11BlendState* alphaBlend = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA);

	RenderTarget ssRenderTarget;

	ID3D11RenderTargetView** worldRenderTarget = superSampleScale == 1 ? &backBuffer : &ssRenderTarget.rt;

	f32 blendFactor[4] {1,1,1,1};

#define PATH_SAVE DATA "world.save"

	World world(renderer, playerPos.chunkPos, chunkDrawDistance);

	std::thread chunkLoader([&]() { while (world.updateChunks()); });
	std::thread meshGenerator([&]() { while (world.generateMeshes()); });

	{
		//FILE* file = fopen(PATH_SAVE, "rb");
		//if (file) {
		//	u32 blockCount;
		//	fread(&blockCount, sizeof(blockCount), 1, file);
		//	for (u32 i= 0; i < blockCount; ++i) {
		//		decltype(world.blocks)::iterator::value_type val;
		//		fread(&val, sizeof(val), 1, file);
		//		world.blocks.insert(val);
		//	}
		//	fclose(file);
		//}
		//else {

		//}
	}


	auto loadWorld = [&]() {
		TIMED_SCOPE("LOAD WORLD");
		auto playerChunk = playerPos.chunkPos;
		//world.loadQueue.mutex.lock();
#if 0
		for (int x = chunkDrawDistance; x <= chunkDrawDistance; ++x) {
			for (int y = chunkDrawDistance; y <= chunkDrawDistance; ++y) {
				for (int z = chunkDrawDistance; z <= chunkDrawDistance; ++z) {
					world.seeChunk(playerChunk + V3i {x,y,z});
				}
			}
		}
#else 
		for (int r = 0; r <= chunkDrawDistance; ++r) {
			{
				int x = -r;
				while (1) {
					for (int y = -r + 1; y <= r - 1; ++y) {
						for (int z = -r + 1; z <= r - 1; ++z) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (x == r)
						break;
					x = r;
				}
			}
			{
				int z = -r;
				while (1) {
					for (int x = -r; x <= r; ++x) {
						for (int y = -r + 1; y <= r - 1; ++y) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (z == r)
						break;
					z = r;
				}
			}
			{
				int y = -r;
				while (1) {
					for (int x = -r; x <= r; ++x) {
						for (int z = -r; z <= r; ++z) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (y == r)
						break;
					y = r;
				}
			}
		}
#endif
		//world.loadQueue.mutex.unlock();
		world.unseeChunks();
	};
	loadWorld();

	enum class MoveMode {
		walk,
		fly,
		noclip,
		count
	};

	MoveMode moveMode {};

	BlockID toolBlock = BLOCK_DIRT;

	MSG msg {};

	float targetFrameTime = 1.0f / 60.0f;

	auto counterFrequency = WH::getPerformanceFrequency();

	bool sleepIsAccurate = timeBeginPeriod(1) == TIMERR_NOERROR;
	if (sleepIsAccurate)
		puts("Sleep() is accurate");
	else
		puts("Sleep() is inaccurate!!!");

#define GRAVITY 13
	bool grounded = false;

	Input input;

	f32 stepLerp = 0.0f;

	char windowTitle[256] {};

	auto lastCounter = WH::getPerformanceCounter();
	while (window.running) {
		input.swap();
		while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
			switch (msg.message) {
				case WM_KEYUP:
				case WM_KEYDOWN:
				case WM_SYSKEYUP:
				case WM_SYSKEYDOWN: {
					u8 code = (u8)msg.wParam;
					assert(code == msg.wParam);

					bool extended   =  (msg.lParam & (1 << 24)) != 0;
					bool context    =  (msg.lParam & (1 << 29)) != 0;
					bool previous   =  (msg.lParam & (1 << 30)) != 0;
					bool transition =  (msg.lParam & (1 << 31)) != 0;
					if (previous == transition) { // Don't handle repeated
						//printf("INPUT: Code: 0X%x, Char: %c, extended: %i, context: %i, previous: %i, transition: %i\n", 
						//       Code, Code, extended, context, previous, transition);
						input.current.keys[code] = !previous;
						if (context && code == VK_F4) {
							window.running = false;
						}
					}
					continue;
				}
				case WM_INPUT: {
					UINT size;
					GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
					assert(size <= 64);
					BYTE buffer[64];

					if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) != size) {
						puts("GetRawInputData did not return correct size!");
						assert(0);
					}

					RAWINPUT& raw = *(RAWINPUT*)buffer;

					if (raw.header.dwType == RIM_TYPEMOUSE) {
						RAWMOUSE& mouse = raw.data.mouse;
						input.current.mouseDelta.x += mouse.lLastX;
						input.current.mouseDelta.y += mouse.lLastY;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) input.current.mouse[0] = 1;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)   input.current.mouse[0] = 0;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) input.current.mouse[1] = 1;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)   input.current.mouse[1] = 0;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) input.current.mouse[2] = 1;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)   input.current.mouse[2] = 0;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) input.current.mouse[3] = 1;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)   input.current.mouse[3] = 0;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) input.current.mouse[4] = 1;
						if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)   input.current.mouse[4] = 0;
					}
					continue;
				}
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
			if (window.killFocus) {
				window.killFocus = false;
				memset(input.current.keys, 0, sizeof(input.current.keys));
				memset(input.current.mouse, 0, sizeof(input.current.mouse));
			}
		}

		stepLerp = max(stepLerp - targetFrameTime * 10, 0.0f);

		if (input.keyDown('V'))
			moveMode = (MoveMode)(((int)moveMode + 1) % (int)MoveMode::count);

		cameraRot.x -= input.mouseDelta().y * 0.0005f;
		cameraRot.y += input.mouseDelta().x * 0.0005f;

		V3 mov;
		mov.x += input.keyHeld('D');
		mov.x -= input.keyHeld('A');
		mov.z += input.keyHeld('W');
		mov.z -= input.keyHeld('S');
		if (moveMode != MoveMode::walk) {
			mov.y += input.keyHeld('E');
			mov.y -= input.keyHeld('Q');
		}
		auto lengthSqr = mov.lengthSqr();
		if (lengthSqr > 0) {
			mov /= sqrtf(lengthSqr);
		}
		if (moveMode != MoveMode::walk) {
			mov = M4::rotationZXY(-cameraRot) * mov;
		}
		else {
			mov = M4::rotationY(-cameraRot.y) * mov;
		}

		f32 accelMult = 1.0f;
		V3 acceleration;

		if(moveMode == MoveMode::walk)
			acceleration.y -= GRAVITY;

		if (grounded) {
			acceleration.y += input.keyHeld(' ') * jumpForce;
		}
		else {
			if (moveMode == MoveMode::noclip) {
				accelMult *= noclipMult;
			}
			else if (moveMode == MoveMode::walk) {
				accelMult *= airMult;
			}
		}

		if (moveMode != MoveMode::walk)
			accelMult *= (noclipMaxSpeed - playerVel.length()) / noclipMaxSpeed;
		else
			accelMult *= (maxSpeed - playerVel.xz().length()) / maxSpeed;

		if (input.keyHeld(VK_SHIFT))
			accelMult *= 2;

		acceleration += mov * targetFrameTime * accelMult * 2500;

		playerVel += acceleration * targetFrameTime * 2;
		if (moveMode != MoveMode::walk) {
			f32 l = playerVel.length();
			if (l > 0) {
				playerVel /= l;
				l -= noclipFriction;
				if (l < 0)
					playerVel = 0;
				else
					playerVel *= l;
			}
		}
		else {
			V2 v = {playerVel.x, playerVel.z};
			f32 l = v.length();
			if (l > 0) {
				v /= l;
				l -= grounded ? groundFriction : airFriction;
				if (l < 0)
					v = 0;
				else
					v *= l;
			}
			playerVel.x = v.x;
			playerVel.z = v.y;
		}

		auto raycast = [&](V3 begin, V3 end, Hit& outHit, V3i& outBlock, auto&& predicate, V3 extent = 0) {
			V3 min, max;
			minmax(begin.x, end.x, min.x, max.x);
			minmax(begin.y, end.y, min.y, max.y);
			minmax(begin.z, end.z, min.z, max.z);
			min -= extent;
			max += extent;
			V3i mini = r2w(min.rounded(), playerPos.chunkPos);
			V3i maxi = r2w(max.rounded(), playerPos.chunkPos);
			f32 minDist = FLT_MAX;
			bool wasHit = false;
			for (int z = mini.z; z <= maxi.z; ++z) {
				for (int y = mini.y; y <= maxi.y; ++y) {
					for (int x = mini.x; x <= maxi.x; ++x) {
						V3i worldTestBlock = {x,y,z}; //world pos
						if (auto opt = world.getBlock(worldTestBlock); opt.has_value()) {
							if (predicate(*opt)) {
								auto relTestBlock = w2r(worldTestBlock, playerPos.chunkPos);
								Hit hit;
								if (raycastBlock(begin, end, (V3)relTestBlock, hit, extent + 0.5f)) {
									auto lenSqr = (begin - hit.p).lengthSqr();
									if (lenSqr < minDist) {
										minDist = lenSqr;
										outHit = hit;
										outBlock = relTestBlock;
										wasHit = true;
									}
								}
							}
						}
					}
				}
			}
			return wasHit;
		};

		auto isInsideBlock = [&](V3 pos, V3 extent, V3i block) {
			V3i mini = (pos - extent).rounded();
			V3i maxi = (pos + extent).rounded();
			return 
				mini.x <= block.x && block.x <= maxi.x &&
				mini.y <= block.y && block.y <= maxi.y &&
				mini.z <= block.z && block.z <= maxi.z;
		};
		auto isInsideWorldBlock = [&](V3 pos, V3 extent) {
			V3i mini = r2w((pos - extent).rounded(), playerPos.chunkPos);
			V3i maxi = r2w((pos + extent).rounded(), playerPos.chunkPos);
			for (int z = mini.z; z <= maxi.z; ++z) {
				for (int y = mini.y; y <= maxi.y; ++y) {
					for (int x = mini.x; x <= maxi.x; ++x) {
						if (!world.canWalkInto({x,y,z})) {
							return true;
						}
					}
				}
			}
			return false;
		};

		auto newPlayerPos = playerPos;
		if (moveMode == MoveMode::noclip) {
			grounded = false;
		} 
		else {
			// raycast
			bool newGrounded = false;
			for (int iter = 0; iter < 4; ++iter) {
				V3 begin = newPlayerPos.relPos;
				V3 end = begin + playerVel * targetFrameTime;
				f32 targetTravelLenSqr = (begin - end).lengthSqr();
				if (targetTravelLenSqr == 0)
					break;
				Hit hit;
				V3i relHitBlock;
				if (raycast(begin, end, hit, relHitBlock, [&](const Block& b) { return !isTransparent(b.id); }, playerDimH)) {
					f32 ndotup = hit.n.dot({0,1,0});
					if (ndotup > 0.5f)
						newGrounded = moveMode != MoveMode::fly;
					if (grounded && fabsf(ndotup) < 1e-1f) {

						f32 x = 0.5f + playerDimH.x;
						f32 y = 0.5f + playerDimH.y;
						f32 z = 0.5f + playerDimH.z;
						auto b = hit.p - hit.n * 1e-3f;
						auto a = b + V3 {0,1.0001f,0};
						Hit stepHit;
						V3 hitBlockF = (V3)relHitBlock;
						if (raycastPlane(a, b, hitBlockF + V3 {x, y, z}, hitBlockF + V3 {x, y,-z}, hitBlockF + V3 {-x, y, z}, stepHit)) {
							auto newPos = stepHit.p + V3 {0,1e-3f,0};
							if (!isInsideWorldBlock(newPos, playerDimH)) {
								newPlayerPos.relPos = newPos;
								stepLerp = 1.0f;
								continue;
							}
						}
					}
					assert(hit.n.lengthSqr() == 1);
					newPlayerPos.relPos = hit.p + hit.n * 1e-3f;
					playerVel -= hit.n * hit.n.dot(playerVel);
				}
				else {
					break;
				}
			}
			grounded = newGrounded;
		}
		if (input.keyDown('T')) {
			V3 command;
			SetForegroundWindow(GetConsoleWindow());
			std::cin >> command.x;
			std::cin >> command.y;
			std::cin >> command.z;
			SetForegroundWindow(window.hwnd);
			newPlayerPos.relPos = w2r(command, newPlayerPos.chunkPos);
		}
		else {
			newPlayerPos.relPos += playerVel * targetFrameTime;
		}

#if 0
		if (!grounded && newPlayerPos.chunkPos.y < -1) {
			newPlayerPos = spawnPos;
			playerVel = 0;
		}
#endif
		playerPos = newPlayerPos;
		auto chunkChanged = playerPos.normalize();

		if (chunkChanged) {
			loadWorld();
		}

		auto cameraPos = playerPos.relPos;
		cameraPos.y += camHeight;
		cameraPos.y -= stepLerp;
		V3 viewDir = M4::rotationZXY(-cameraRot) * V3 { 0, 0, 1 };
		auto matrixVP = projection * M4::rotationYXZ(cameraRot) * M4::translation(-cameraPos);

		frameCBufferData.camPos = cameraPos;
		renderer.updateBuffer(frameCBuffer, frameCBufferData);

		if (window.resize) {
			window.resize = false;

			if (backBuffer) {
				backBuffer->Release();
				depthView->Release();
				depthTex->Release();
			}

			DHR(renderer.swapChain->ResizeBuffers(2, window.clientSize.x, window.clientSize.y, DXGI_FORMAT_UNKNOWN, 0));

			ID3D11Texture2D* backBufferTexture = 0;
			DHR(renderer.swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture)));
			DHR(renderer.device->CreateRenderTargetView(backBufferTexture, 0, &backBuffer));
			backBufferTexture->Release();

			D3D11_TEXTURE2D_DESC desc {};
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			desc.Width = window.clientSize.x * superSampleScale;
			desc.Height = window.clientSize.y * superSampleScale;
			desc.MipLevels = 1;
			desc.SampleDesc = {1, 0};
			DHR(renderer.device->CreateTexture2D(&desc, 0, &depthTex));
			DHR(renderer.device->CreateDepthStencilView(depthTex, 0, &depthView));

			projection = M4::projection((f32)window.clientSize.x / window.clientSize.y, DEG2RAD(90), 0.01f, farClipPlane);

			if (superSampleScale != 1) {
				if (ssRenderTarget)
					ssRenderTarget.Release();
				ssRenderTarget = renderer.createRenderTarget(window.clientSize * superSampleScale);

				f32 ssOff = 0.0f;
				switch (superSampleScale) {
					case 2: ssOff = 0.25f; break;
					case 3: ssOff = 1.0f / 3.0f; break;
					case 4: ssOff = 0.125f; break;
				}
				blitCBufferData.sampleOffset = V2 {1} / V2 {window.clientSize} * ssOff;
				renderer.updateBuffer(blitCBuffer, blitCBufferData);
			}
		}

		renderer.deviceContext->OMSetRenderTargets(1, worldRenderTarget, depthView);
		renderer.deviceContext->ClearRenderTargetView(*worldRenderTarget, &clearColor.x);
		renderer.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
		renderer.setViewport(window.clientSize * superSampleScale);
		renderer.deviceContext->VSSetShader(blockVS, 0, 0);
		renderer.deviceContext->PSSetShader(blockPS, 0, 0);
		renderer.deviceContext->PSSetShaderResources(2, 1, &atlasTex);

		struct FrustumPlanes {
			V4 frustumPlanes[6];
			constexpr FrustumPlanes(const M4& vp) noexcept {
				frustumPlanes[0].x = vp.i.w + vp.i.x;
				frustumPlanes[0].y = vp.j.w + vp.j.x;
				frustumPlanes[0].z = vp.k.w + vp.k.x;
				frustumPlanes[0].w = vp.l.w + vp.l.x;
				frustumPlanes[1].x = vp.i.w - vp.i.x;
				frustumPlanes[1].y = vp.j.w - vp.j.x;
				frustumPlanes[1].z = vp.k.w - vp.k.x;
				frustumPlanes[1].w = vp.l.w - vp.l.x;
				frustumPlanes[2].x = vp.i.w - vp.i.y;
				frustumPlanes[2].y = vp.j.w - vp.j.y;
				frustumPlanes[2].z = vp.k.w - vp.k.y;
				frustumPlanes[2].w = vp.l.w - vp.l.y;
				frustumPlanes[3].x = vp.i.w + vp.i.y;
				frustumPlanes[3].y = vp.j.w + vp.j.y;
				frustumPlanes[3].z = vp.k.w + vp.k.y;
				frustumPlanes[3].w = vp.l.w + vp.l.y;
				frustumPlanes[5].x = vp.i.w - vp.i.z;
				frustumPlanes[5].y = vp.j.w - vp.j.z;
				frustumPlanes[5].z = vp.k.w - vp.k.z;
				frustumPlanes[5].w = vp.l.w - vp.l.z;
				frustumPlanes[4].x = vp.i.z;
				frustumPlanes[4].y = vp.j.z;
				frustumPlanes[4].z = vp.k.z;
				frustumPlanes[4].w = vp.l.z;
			}
			/*
			constexpr void Normalize() & noexcept {
				for (auto& p : frustumPlanes) {
					float length = Vec3f(p.x, p.y, p.z).Length();
					p.x /= length;
					p.y /= length;
					p.z /= length;
					p.w /= length;
				}
			}
			*/
			constexpr bool containsSphere(V3 position, float radius) const noexcept {
				for (auto& p : frustumPlanes) {
					if (V3 {p.x, p.y, p.z}.dot(position) + p.w + radius < 0) {
						return false;
					}
				}
				return true;
			}
		};
		FrustumPlanes frustumPlanes { matrixVP };
		u32 chunksVisible = 0;
		for (auto& [pos,c] : world.loadedChunks) {
			if (frustumPlanes.containsSphere(V3 {(c->position - playerPos.chunkPos) * CHUNK_SIZE + CHUNK_SIZE / 2}, CHUNK_SIZE * ROOT3)) {
				++chunksVisible;
				c->draw(playerPos.chunkPos, matrixVP, drawCBuffer);
			}
		}
		//for (auto& c : world.loadedChunks) {
		//	drawChunk(c.second);
		//}
		V3i placePos = (cameraPos + viewDir * 3).rounded();
		V3i breakPos = placePos;
		{
			Hit hit;
			if (raycast(cameraPos, cameraPos + viewDir * 3, hit, placePos, [](const Block& b) {return b.id != BLOCK_AIR; })) {
				breakPos = placePos;
				placePos += (V3i)hit.n;
				renderer.deviceContext->OMSetBlendState(alphaBlend, blendFactor, 0xFFFFFFFF);
				drawCBufferData.model = M4::translation((V3)breakPos) * M4::scaling(1.01f);
				drawCBufferData.mvp = matrixVP * drawCBufferData.model;
				drawCBufferData.solidColor = {0,0,0,0.5};
				renderer.updateBuffer(drawCBuffer, drawCBufferData);
				renderer.deviceContext->PSSetShaderResources(2, 1, &selectionTex);
				blockMesh.draw(renderer);
				renderer.deviceContext->OMSetBlendState(0, blendFactor, 0xFFFFFFFF);
				if (input.mouseDown(0)) {
					if (world.setBlock(breakPos + playerPos.chunkPos * CHUNK_SIZE, {BLOCK_AIR}, true))
						puts("Destroyed!");
				}
				if (input.mouseDown(1)) {
					if (!isInsideBlock(playerPos.relPos, playerDimH, placePos) && world.setBlock(placePos + playerPos.chunkPos * CHUNK_SIZE, {toolBlock}, true))
						puts("Placed!");
				}
			}
		}

		if (superSampleScale != 1) {
			renderer.deviceContext->OMSetRenderTargets(1, &backBuffer, 0);
			renderer.setViewport(window.clientSize);
			renderer.deviceContext->VSSetShader(blitVS, 0, 0);
			renderer.deviceContext->PSSetShader(blitPS, 0, 0);
			renderer.deviceContext->PSSetShaderResources(0, 1, &ssRenderTarget.sr);
			renderer.deviceContext->Draw(3, 0);
			ID3D11ShaderResourceView* null = 0;
			renderer.deviceContext->PSSetShaderResources(0, 1, &null);
		}

		if (input.keyDown(VK_ESCAPE)) {
			ClipCursor(0);
			SetCursor(LoadCursorA(0, IDC_ARROW));
		}
		if (input.keyDown('1')) {
			toolBlock = BLOCK_DIRT;
			puts("Block: BLOCK_DIRT");
		}
		else if (input.keyDown('2')) {
			toolBlock = BLOCK_GRASS;
			puts("Block: BLOCK_GRASS");
		}
#if 0
		auto workCounter = WH::getPerformanceCounter();
		auto workSecondsElapsed = WH::getSecondsElapsed(lastCounter, workCounter, counterFrequency);
		if (workSecondsElapsed < targetFrameTime) {
			if (sleepIsAccurate) {
				i32 msToSleep = (i32)((targetFrameTime - workSecondsElapsed) * 1000.0f);
				if (msToSleep > 0) {
					Sleep((DWORD)msToSleep);
				}
			}
			while (workSecondsElapsed < targetFrameTime) {
				workSecondsElapsed = WH::getSecondsElapsed(lastCounter, WH::getPerformanceCounter(), counterFrequency);
			}
		}
		else {
			//puts("Low framerate!");
		}
		auto endCounter = WH::getPerformanceCounter();
		f32 frameTime = WH::getSecondsElapsed(lastCounter, endCounter, counterFrequency);
		lastCounter = endCounter;
		DHR(renderer.swapChain->Present(0, 0));
#else
		DHR(renderer.swapChain->Present(1, 0));
#endif

		auto [value, unit] = normalizeBytes<double>(vramUsage.load());
		debugGenerateMutex.lock();
		sprintf(windowTitle, "gstorm - Position: (%i/%i/%i) (%.2f/%.2f/%.2f), Chunks visible: %u, Gen. time: %.3fms, VRAM usage: %.3f %s", 
				playerPos.chunkPos.x, playerPos.chunkPos.y, playerPos.chunkPos.z,
				playerPos.relPos.x, playerPos.relPos.y, playerPos.relPos.z,
				chunksVisible, generateMS, value, unit);
		debugGenerateMutex.unlock();
		SetWindowText(window.hwnd, windowTitle);
	}
	world.loadQueueNoPush = true;
	ClipCursor(0);
	SetCursor(LoadCursorA(0, IDC_ARROW));
	DestroyWindow(window.hwnd);
	puts("Waiting for chunk loader thread...");
	chunkLoader.join();
	puts("Waiting for mesh generator thread...");
	meshGenerator.join();

	world.save();
	/*{
		FILE* file = fopen(PATH_SAVE, "wb");
		auto write = [&](const auto& val) {
			fwrite(&val, sizeof(val), 1, file);
		};
		write((u32)world.blocks.size());
		for (auto& b : world.blocks) {
			write(b);
		}
		fclose(file);
	}*/

	return 0;
}