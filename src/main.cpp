// TODO: 
// Free memory of far chunks, but left them visible
// Make approximate meshes for far chunks
// BUG: chunk saved twice

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
#pragma comment(lib, "winmm")
#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_VISIBLE)
#define DATA "../data/"
#define LDATA L"../data/"
#define INVALID_BLOCK_POS INT32_MIN
#define FILEPOS_INVALID ((FilePos)1)
#define FILEPOS_APPEND ((FilePos)2)
#include "borismap.h"
#include <concurrent_queue.h>
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
struct ConcurrentQueue {
	void push(T chunk) {
		std::unique_lock l(mutex);
		buffer.push(chunk);
	}
	std::optional<T> pop() {
		std::unique_lock l(mutex);
		if (!buffer.size()) {
			return {};
		}
		DEFER {
			buffer.pop();
		};
		return buffer.front();
	}
private:
	std::queue<T> buffer;
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

#define CHUNK_WIDTH 32
#define CHUNK_VOLUME (CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH)
#define FOR_BLOCK_IN_CHUNK 		    \
for (int z=0; z < CHUNK_WIDTH; ++z) \
for (int x=0; x < CHUNK_WIDTH; ++x) \
for (int y=0; y < CHUNK_WIDTH; ++y) 
#define BLOCK_INDEX(x,y,z) ((z) * CHUNK_WIDTH * CHUNK_WIDTH + (x) * CHUNK_WIDTH + (y))

#include "../data/shaders/chunkVertex.h"
struct alignas(16) DrawCBuffer {
	M4 mvp;
	M4 model;
	V4 solidColor;
};
struct alignas(16) FrameCBuffer {
	V3 camPos;
	f32 time;

	V3 lightDir;
	f32 pad;

	V3 skyColor;
	f32 earthBloomMult;

	V3 sunColor;
	f32 sunBloomMult;
};
struct alignas(16) SceneCBuffer {
	f32 fogDistance = 0;
};
struct alignas(16) ScreenCBuffer {
	V2 sampleOffset;
	V2 invScreenSize;
};
template<size_t slot, class Data>
struct CBuffer : Data {
	enum class State {
		invalid, default, dynamic, immutable
	};
	static_assert(alignof(Data) % 16 == 0);
	ID3D11Buffer* buffer = 0;
	Renderer& renderer;
	State state = State::invalid;
	CBuffer(Renderer& renderer) : renderer(renderer) {}
	void createDynamic() {
		state = State::dynamic;
		buffer = renderer.createDynamicBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(Data), 0, 0);
	}
	void createImmutable() {
		state = State::immutable;
		buffer = renderer.createImmutableBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(Data), 0, 0, this);
	}
	void update() {
		renderer.updateBuffer(buffer, this, sizeof(Data));
	}
	void bindV() {
		renderer.deviceContext->VSSetConstantBuffers(slot, 1, &buffer);
	}
	void bindP() {
		renderer.deviceContext->PSSetConstantBuffers(slot, 1, &buffer);
	}
};
struct Vertex {
	V3 position;
	V2 uv;
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
		renderer.draw(vertexCount);
	}
};
Mesh blockMesh;
#define CHUNK_SIZE (sizeof(BlockID) * CHUNK_VOLUME)
#define SAVE_FILE DATA "save/world"
void printChunk(BlockID*, V3i);
V3i chunkPosFromBlock(V3i pos) {
	return floor(pos, CHUNK_WIDTH) / CHUNK_WIDTH;
}
V3i chunkPosFromBlock(V3 pos) {
	return chunkPosFromBlock(V3i {pos});
}
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
std::atomic<size_t> ramUsage, vramUsage;
i64 generateTime, generateCount;
i64 buildMeshTime, buildMeshCount;
std::mutex debugGenerateMutex, debugBuildMeshMutex;
#define MAX_CHUNK_VERTEX_COUNT (1024 * 1024)
std::atomic_uint meshesBuilt = 0;
struct Chunk {
	struct VertexBuffer {
		ID3D11Buffer* vBuffer = 0;
		ID3D11ShaderResourceView* vBufferView = 0;
		u32 vertexCount = 0;
		void free() {
			if (vBuffer) { vBuffer->Release();     vBuffer     = 0; }
			if (vBufferView) { vBufferView->Release(); vBufferView = 0; }
			vramUsage -= vertexCount * sizeof(ChunkVertex);
			vertexCount = 0;
		}
	};
	struct MeshBuffer {
		VertexBuffer buffers[2], * current = buffers, * next = buffers + 1;
		void swap() {
			std::swap(current, next);
		}
		void free() {
			for (auto& b : buffers) {
				b.free();
			}
		}
	} meshBuffer;
	VertexBuffer approxMesh;
	BlockID* blocks = 0;
	Renderer& renderer;
	V3i position;
	bool needToSave = false;
	bool meshGenerated = false;
	bool generated = false;
	bool wantedToBeDeleted = false;
	std::atomic_int userCount = 0;
	std::mutex deleteMutex;
	std::mutex bufferMutex;
	Chunk(V3i position, Renderer& renderer) : position(position), renderer(renderer) {
		ramUsage += sizeof(Chunk);
	}
	~Chunk() {
		ramUsage -= sizeof(Chunk);
		assert(userCount == 0);
	}
	void allocateBlocks() {
		if (!blocks) {
			blocks = new BlockID[CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH] {};
			ramUsage += CHUNK_SIZE;
		}
	}
	void freeBlocks() {
		if (blocks) {
			delete blocks;
			blocks = 0;
			ramUsage -= CHUNK_SIZE;
		}
	}
	void free() {
		freeBlocks();
		meshBuffer.free();
		//approxMesh.free();
	}
	FilePos filePos = FILEPOS_INVALID;
	bool load(FILE* chunkFile) {
		//TIMED_FUNCTION_;
		assert(chunkFile);

		filePos = BorisHash::find(position);
		if (filePos == FILEPOS_INVALID)
			return false;

		assert(filePos % CHUNK_SIZE == 0);
		_fseeki64(chunkFile, filePos, SEEK_SET);
		allocateBlocks();
		fread(blocks, CHUNK_SIZE, 1, chunkFile);
		generated = true;
		return true;
	}
	void save(FILE* chunkFile) {
		//TIMED_FUNCTION_;
		assert(chunkFile);
		if (filePos == FILEPOS_INVALID || !needToSave) {
			return;
		}
		needToSave = false;
		assert(blocks);

		filePos = BorisHash::find(position);
		if (filePos == FILEPOS_INVALID)
			filePos = FILEPOS_APPEND;

		if (filePos == FILEPOS_APPEND) {
			_fseeki64(chunkFile, 0, SEEK_END);
			filePos = _ftelli64(chunkFile);
			assert(filePos % CHUNK_SIZE == 0);

			fwrite(blocks, CHUNK_SIZE, 1, chunkFile);

			BorisHash::add({position, filePos});
		}
		else {
			assert(filePos % CHUNK_SIZE == 0);
			_fseeki64(chunkFile, filePos, SEEK_SET);
			fwrite(blocks, CHUNK_SIZE, 1, chunkFile);
		}
	}
	// returns true if block changed
	bool setBlock(int x, int y, int z, BlockID blk) {
		assert(x >= 0 && x < CHUNK_WIDTH);
		assert(y >= 0 && y < CHUNK_WIDTH);
		assert(z >= 0 && z < CHUNK_WIDTH);
		auto& b = blocks[BLOCK_INDEX(x,y,z)];
		auto result = b != blk;
		b = blk;
		needToSave = true;
		meshGenerated = false;
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
		return blocks[BLOCK_INDEX(x, y, z)];
	}
	BlockID& getBlock(V3i p) {
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
		if (position.y < 0) {
			memset(blocks, 0x01010101, CHUNK_SIZE);
		}
		else {
			auto beginCounter = WH::getPerformanceCounter();
			for (int x = 0; x < CHUNK_WIDTH; ++x) {
				for (int z = 0; z < CHUNK_WIDTH; ++z) {
					V2 globalPos = V2 {(f32)(position.x * CHUNK_WIDTH + x), (f32)(position.z * CHUNK_WIDTH + z)};
					int h = 0;
					//h += (int)(128 - perlin(seed + 100000, 8) * 256);
					h += (int)(textureDetail(8, globalPos / 256.0f / PI, voronoi) * 512) + 2;
					h -= position.y * CHUNK_WIDTH;
					auto top = h;
					h = clamp(h, 0, CHUNK_WIDTH);
					for (int y = 0; y < h; ++y) {
						auto b = BLOCK_DIRT;
						if (y == top - 1) {
							//b = BLOCK_TALL_GRASS;
							b = perlin(globalPos / PI, 2) > 0.5f ? BLOCK_TALL_GRASS : BLOCK_AIR;
						}
						if (y == top - 2) b = BLOCK_GRASS;
						blocks[BLOCK_INDEX(x, y, z)] = b;
					}
				}
			}
			auto endCounter = WH::getPerformanceCounter();
			debugGenerateMutex.lock();
			generateTime += endCounter - beginCounter;
			++generateCount;
			debugGenerateMutex.unlock();
		}
		generated = true;
		
		if(1) //if (!saveSpaceOnDisk)
			needToSave = true; // TODO: make option for saving space on disk
	}
	/*
	void buildApproxMesh() {
		u32 heights[4] {};
		for (int y=0; y < CHUNK_WIDTH; ++y) { if()heights[0] = max(); }
		struct Vertex {
			V3 pos, nrm;
			V2 uv;
		};
		int pos = blockInfos.at(BLOCK_GRASS).offsetAtlasPos(AXIS_PY);
		V2i atlasPos {
			pos % ATLAS_SIZE,
			pos / ATLAS_SIZE,
		};
		V2 uvs[4] {
			V2{atlasPos.x,  atlasPos.y} * ATLAS_ENTRY_SIZE,
			V2{atlasPos.x + 1,atlasPos.y} * ATLAS_ENTRY_SIZE,
			V2{atlasPos.x + 1,atlasPos.y + 1} * ATLAS_ENTRY_SIZE,
			V2{atlasPos.x,  atlasPos.y + 1} * ATLAS_ENTRY_SIZE,
		};
		auto uvIdx = randomU32(position) % 4;
		Vertex verts[4] {
			{{0, (f32)heights[0], CHUNK_WIDTH}		    ,{0,1,0}, uvs[(uvIdx + 0) % 4]},
			{{CHUNK_WIDTH, (f32)heights[1], CHUNK_WIDTH},{0,1,0}, uvs[(uvIdx + 1) % 4]},
			{{0, (f32)heights[2], 0}					,{0,1,0}, uvs[(uvIdx + 3) % 4]},
			{{CHUNK_WIDTH, (f32)heights[3], 0}		    ,{0,1,0}, uvs[(uvIdx + 2) % 4]},
		};

		Vertex vertices[6] {
			verts[0],
			verts[1],
			verts[2],
			verts[1],
			verts[3],
			verts[2],
		}
		approxMesh.free();
		approxMesh.vertexCount = 6;

		if (approxMesh.vertexCount) {
			u32 vertexBufferSize = approxMesh.vertexCount * sizeof(vertices[0]);
			approxMesh.vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, sizeof(vertices[0]), vertices);
			vramUsage += vertexBufferSize;

			D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
			desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			desc.Buffer.NumElements = approxMesh.vertexCount;
			DHR(renderer.device->CreateShaderResourceView(approxMesh.vBuffer, &desc, &approxMesh.vBufferView));
		}
	}
	void drawApprox(ID3D11Buffer* drawCBuffer) {
		renderer.deviceContext->VSSetShaderResources(0, 1, &approxMesh.vBufferView);
		renderer.updateBuffer(drawCBuffer, drawData);
		renderer.draw(approxMesh.vertexCount);
	}
	*/
	void generateMesh(ChunkVertex* vertexPool, const std::array<Chunk*, 6>& neighbors) {
		deleteMutex.lock();
		if (wantedToBeDeleted) {
			deleteMutex.unlock();
			return;
		}
		deleteMutex.unlock();
		//TIMED_FUNCTION;
		assert(blocks);
		ChunkVertex* verticesBegin = vertexPool;
		auto pushVertex = [&vertexPool, verticesBegin](ChunkVertex vert) {
			assert(vertexPool - verticesBegin < MAX_CHUNK_VERTEX_COUNT && "small pool");
			*vertexPool++ = vert;
		};
		auto beginCounter = WH::getPerformanceCounter();
//#define FIRST
#ifndef FIRST
		u8 transparent[6][CHUNK_WIDTH][CHUNK_WIDTH];
#define FILL_NEIGHBOR_BLOCKS(axis, iterA, iterB, x, y, z)														\
		if (neighbors[axis])																					\
			for (u32 iterA = 0; iterA < CHUNK_WIDTH; ++iterA) for (u32 iterB = 0; iterB < CHUNK_WIDTH; ++iterB)	\
				transparent[axis][iterA][iterB] = isTransparent(neighbors[axis]->getBlock(x, y, z));			\
		else																									\
			memset(transparent[axis], 1, sizeof(transparent[axis]));
		
		FILL_NEIGHBOR_BLOCKS(0, y, z,               0, y, z);
		FILL_NEIGHBOR_BLOCKS(1, y, z, CHUNK_WIDTH - 1, y, z);
		FILL_NEIGHBOR_BLOCKS(2, x, z, x,               0, z);
		FILL_NEIGHBOR_BLOCKS(3, x, z, x, CHUNK_WIDTH - 1, z);
		FILL_NEIGHBOR_BLOCKS(4, x, y, x, y,               0);
		FILL_NEIGHBOR_BLOCKS(5, x, y, x, y, CHUNK_WIDTH - 1);
#endif
		//u32 idxOff = 0;
		FOR_BLOCK_IN_CHUNK {
			auto & b = getBlock(x,y,z);
			switch (b) {
				case 0:
					continue;
			}
			auto& blockInfo = blockInfos.at(b);

			switch (blockInfo.type) {
				case BlockInfo::Type::default:
				case BlockInfo::Type::topSideBottom: {
					bool visible[6];
#ifdef FIRST
					visible[0] = x == CHUNK_WIDTH - 1 ? (neighbors[0] ? isTransparent(neighbors[0]->getBlock(              0, y, z)) : true) : isTransparent(getBlock(x + 1, y, z));
					visible[1] =               x == 0 ? (neighbors[1] ? isTransparent(neighbors[1]->getBlock(CHUNK_WIDTH - 1, y, z)) : true) : isTransparent(getBlock(x - 1, y, z));
					visible[2] = y == CHUNK_WIDTH - 1 ? (neighbors[2] ? isTransparent(neighbors[2]->getBlock(x,               0, z)) : true) : isTransparent(getBlock(x, y + 1, z));
					visible[3] =               y == 0 ? (neighbors[3] ? isTransparent(neighbors[3]->getBlock(x, CHUNK_WIDTH - 1, z)) : true) : isTransparent(getBlock(x, y - 1, z));
					visible[4] = z == CHUNK_WIDTH - 1 ? (neighbors[4] ? isTransparent(neighbors[4]->getBlock(x, y,               0)) : true) : isTransparent(getBlock(x, y, z + 1));
					visible[5] =               z == 0 ? (neighbors[5] ? isTransparent(neighbors[5]->getBlock(x, y, CHUNK_WIDTH - 1)) : true) : isTransparent(getBlock(x, y, z - 1));
#else
					visible[0] = x == CHUNK_WIDTH - 1 ? transparent[0][y][z] : isTransparent(getBlock(x + 1, y, z));
					visible[1] =               x == 0 ? transparent[1][y][z] : isTransparent(getBlock(x - 1, y, z));
					visible[2] = y == CHUNK_WIDTH - 1 ? transparent[2][x][z] : isTransparent(getBlock(x, y + 1, z));
					visible[3] =               y == 0 ? transparent[3][x][z] : isTransparent(getBlock(x, y - 1, z));
					visible[4] = z == CHUNK_WIDTH - 1 ? transparent[4][x][y] : isTransparent(getBlock(x, y, z + 1));
					visible[5] =               z == 0 ? transparent[5][x][y] : isTransparent(getBlock(x, y, z - 1));
#endif
					auto calcVerts = [&](u8 axis) {
						u32 defData0 = makeVertexData0(x,y,z,0,axis);
						ChunkVertex verts[4] {};
						verts[0].data0 = defData0;
						verts[1].data0 = defData0;
						verts[2].data0 = defData0;
						verts[3].data0 = defData0;
						switch (axis) {
							case AXIS_PX:
								verts[0].setPositionID(1);
								verts[1].setPositionID(0);
								verts[2].setPositionID(3);
								verts[3].setPositionID(2);
								break;
							case AXIS_NX:
								verts[0].setPositionID(4);
								verts[1].setPositionID(5);
								verts[2].setPositionID(6);
								verts[3].setPositionID(7);
								break;
							case AXIS_PY:
								verts[0].setPositionID(4);
								verts[1].setPositionID(0);
								verts[2].setPositionID(5);
								verts[3].setPositionID(1);
								break;
							case AXIS_NY:
								verts[0].setPositionID(7);
								verts[1].setPositionID(3);
								verts[2].setPositionID(6);
								verts[3].setPositionID(2);
								break;
							case AXIS_PZ:
								verts[0].setPositionID(0);
								verts[1].setPositionID(4);
								verts[2].setPositionID(2);
								verts[3].setPositionID(6);
								break;
							case AXIS_NZ:
								verts[0].setPositionID(5);
								verts[1].setPositionID(1);
								verts[2].setPositionID(7);
								verts[3].setPositionID(3);
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
						verts[0].setUvRotation(uvIdx);
						verts[1].setUvRotation(uvIdx);
						verts[2].setUvRotation(uvIdx);
						verts[3].setUvRotation(uvIdx);
						V2i uvs[4];
						uvs[0] = {atlasPos.x,  atlasPos.y};
						uvs[1] = {atlasPos.x + 1,atlasPos.y};
						uvs[2] = {atlasPos.x + 1,atlasPos.y + 1};
						uvs[3] = {atlasPos.x,  atlasPos.y + 1};
						verts[0].setData1(uvs[(uvIdx + 0) % 4]);
						verts[1].setData1(uvs[(uvIdx + 1) % 4]);
						verts[2].setData1(uvs[(uvIdx + 3) % 4]);
						verts[3].setData1(uvs[(uvIdx + 2) % 4]);
						pushVertex(verts[0]);
						pushVertex(verts[1]);
						pushVertex(verts[2]);
						pushVertex(verts[1]);
						pushVertex(verts[3]);
						pushVertex(verts[2]);
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
					ChunkVertex verts[4] {};
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
							verts[0].setData1(uvs[0]).setUvFlip(0b00);
							verts[1].setData1(uvs[1]).setUvFlip(0b00);
							verts[2].setData1(uvs[3]).setUvFlip(0b00);
							verts[3].setData1(uvs[2]).setUvFlip(0b00);
						}
						else {
							verts[0].setData1(uvs[1]).setUvFlip(0b10);
							verts[1].setData1(uvs[0]).setUvFlip(0b10);
							verts[2].setData1(uvs[2]).setUvFlip(0b10);
							verts[3].setData1(uvs[3]).setUvFlip(0b10);
						}
					};
					auto insertVertices = [&](u32 p, u32 a) {
						randomizeUv((p << 1) | a);
						verts[0].data0 = makeVertexData0(x, y, z, 0 + p, AXIS_PY);
						verts[1].data0 = makeVertexData0(x, y, z, 5 - p, AXIS_PY);
						verts[2].data0 = makeVertexData0(x, y, z, 2 + p, AXIS_PY);
						verts[3].data0 = makeVertexData0(x, y, z, 7 - p, AXIS_PY);
						pushVertex(verts[0]);
						pushVertex(verts[a ? 1 : 2]);
						pushVertex(verts[a ? 2 : 1]);
						pushVertex(verts[1]);
						pushVertex(verts[a ? 3 : 2]);
						pushVertex(verts[a ? 2 : 3]);
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
		auto endCounter = WH::getPerformanceCounter();
		debugBuildMeshMutex.lock();
		buildMeshTime += endCounter - beginCounter;
		++buildMeshCount;
		debugBuildMeshMutex.unlock();

		std::unique_lock l(bufferMutex);
		auto& buffer = *meshBuffer.next;

		buffer.free();

		buffer.vertexCount = (u32)(vertexPool - verticesBegin);

		if (!buffer.vertexCount)
			return;

		u32 vertexBufferSize = buffer.vertexCount * sizeof(verticesBegin[0]);
		buffer.vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, sizeof(verticesBegin[0]), verticesBegin);
		vramUsage += vertexBufferSize;

		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = buffer.vertexCount;
		DHR(renderer.device->CreateShaderResourceView(buffer.vBuffer, &desc, &buffer.vBufferView));
		meshGenerated = true;
		++meshesBuilt;
	}
	template<size_t slot>
	void draw(CBuffer<slot, DrawCBuffer>& drawCBuffer, V3i playerChunk, const M4& matrixVP) {
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
			renderer.deviceContext->VSSetShaderResources(0, 1, &buffer->vBufferView);
		}
		drawCBuffer.model = M4::translation((V3)((position - playerChunk) * CHUNK_WIDTH));
		drawCBuffer.mvp = matrixVP * drawCBuffer.model;
		drawCBuffer.update();
		renderer.draw(buffer->vertexCount);
	}
};
/*
void printChunk(Block* blocks, V3i pos) {
	auto usedBlocks = 0;
	for (int j = 0; j < TOTAL_CHUNK_SIZE; ++j) {
		if (blocks[j] == BlockID::cube) ++usedBlocks;
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
	Chunk* allocate() {
		for (u32 i = 0; i < capacity; ++i) {
			if (!used[i]) {
				used[i] = true;
				return new(memory + i) Chunk;
			}
		}
		assert(!"Not enough memory");
	}
	void free(Chunk* ptr) {
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
	std::unordered_map<V3i, Chunk*> loadedChunks;
	ConcurrentQueue<Chunk*> loadQueue, unloadQueue;
	ChunkVertex* vertexPools[2]; // extra one for immediate mesh build
	std::atomic_bool loadQueueNoPush = false;
	std::vector<Chunk*> chunksWantedToDelete;
	Renderer& renderer;
	const V3i& playerChunk;
	const int drawDistance;
	const int hotDistance;
	std::thread chunkLoader;
	FILE* saveFile;
	World(Renderer& renderer, const V3i& playerChunk, int drawDistance) : renderer(renderer), playerChunk(playerChunk), drawDistance(drawDistance), hotDistance(min(drawDistance, HOT_DIST)) {
		int w = drawDistance * 2 + 1;
		loadedChunks.reserve(w * w * w);
		for (auto& pool : vertexPools) {
			pool = (ChunkVertex*)malloc(MAX_CHUNK_VERTEX_COUNT * sizeof(ChunkVertex));
		}
		BorisHash::init();
		saveFile = openFileRW(SAVE_FILE);
		//BorisHash::debug();
		chunkLoader = std::thread {[this]() {
			SetThreadName((DWORD)-1, "Chunk loader");
			while (updateChunks());
		}};
	}
	void seeChunk(V3i pos) {
		getChunkUnchecked(pos);
		//visibleChunks.emplace(getChunkUnchecked(pos));
	}
	// main thread, called when player moves to other chunk
	void unseeChunks() {
		//std::vector<Chunk*> toUnsee;
		//toUnsee.reserve(drawDistance * 2 + 1);
		//for (auto& c : visibleChunks) {
		//	if (maxDistance(playerChunk, c->position) > drawDistance) {
		//		toUnsee.push_back(c);
		//	}
		//}
		//for (auto& c : toUnsee)
		//	visibleChunks.erase(c);

		std::vector<Chunk*> chunksToUnload;
		chunksToUnload.reserve(drawDistance * 2 + 1);
		for (auto& [pos, c] : loadedChunks) {
			auto dist = maxDistance(playerChunk, c->position);
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
		puts("Waiting for chunk loader thread...");
		chunkLoader.join();

		size_t progress = 0;
		for (auto& [position, chunk] : loadedChunks) {
			chunk->save(saveFile);
			chunk->free();
			delete chunk;
			printf("Saving world... %zu%%\r", progress++ * 100 / loadedChunks.size());
		}
		puts("Saving world... 100%");
		fclose(saveFile);
		BorisHash::shutdown();
	}
	std::array<Chunk*, 6> getNeighbors(Chunk* c) {
		std::array<Chunk*, 6> neighbors;
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
	void wantToDelete(Chunk* c) {
		if (!c->wantedToBeDeleted) {
			c->wantedToBeDeleted = true;
			assert_dbg(!contains(chunksWantedToDelete, c));
			chunksWantedToDelete.push_back(c);
		}
	}
	// chunk loader thread
	bool loadChunks() {
		while(1) {
			//printf("To load: %zu\n", loadQueue.size());
			if (auto oc = loadQueue.pop(); oc.has_value()) {
				auto c = oc.value();
				DEFER {
					--c->userCount;
				};
				if (maxDistance(playerChunk, c->position) > drawDistance) {
					continue;
				}
				else {
					c->wantedToBeDeleted = false;
				}
				if (loadQueueNoPush)
					continue;
				if (!saveFile || !c->load(saveFile)) {
					c->generate();
				}
				//c->buildApproxMesh();
				buildMesh(c);
				for (auto& n : getNeighbors(c)) {
					if (n) {
						buildMesh(n);
					}
				}
			}
			else {
				break;
			}
		}
		return !loadQueueNoPush;
	}
	void unloadChunks() {
		//printf("To save: %zu\n", unloadQueue.size());
		while (1) {
			if (auto oc = unloadQueue.pop(); oc.has_value()) {
				auto c = oc.value();
				wantToDelete(c);
			}
			else {
				break;
			}
		}

		std::vector<Chunk*> toDelete;
		std::vector<Chunk*> notToDelete;
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
		//printf("To delete: %zu\n", toDelete.size());
		for (auto& c : toDelete) {
			c->save(saveFile);
			c->free();
			delete c;
		}
	}
	bool updateChunks() {
		unloadChunks();
		return loadChunks();
	}
	void buildMesh(Chunk* c) {
		c->generateMesh(vertexPools[0], getNeighbors(c));
	}
	Chunk* findChunk(V3i pos) {
		if (auto it = loadedChunks.find(pos); it != loadedChunks.end())
			return it->second;
		return 0;
	}
	Chunk* getChunkUnchecked(V3i pos) {
		Chunk* chunk = findChunk(pos);
		if (chunk)
			return chunk;
		chunk = new Chunk(pos, renderer);
		loadedChunks[pos] = chunk;
		++chunk->userCount;
		loadQueue.push(chunk);
		return chunk;
	}
	Chunk* getChunk(V3i pos) {
		auto c = getChunkUnchecked(pos);
		if (c->generated)
			return c;
		return 0;
	}
	Chunk* getChunkFromBlock(V3i pos) {
		return getChunk(chunkPosFromBlock(pos));
	}
	bool setBlock(V3i worldPos, BlockID blk, bool immediate = false) {
		auto chunkPos = chunkPosFromBlock(worldPos);
		auto c = getChunk(chunkPos);
		if (!c)
			return false;
		auto relPos = frac(worldPos, CHUNK_WIDTH);
		if (c->setBlock(relPos, blk)) {
			auto updateMesh = [this, immediate](Chunk* chunk) {
				if (immediate) {
					chunk->generateMesh(vertexPools[1], getNeighbors(chunk));
				}
				else
					buildMesh(chunk);
			};
			updateMesh(c);
			auto onBoundary = [](V3i p) {
				return
					p.x == 0 || p.x == CHUNK_WIDTH - 1 ||
					p.y == 0 || p.y == CHUNK_WIDTH - 1 ||
					p.z == 0 || p.z == CHUNK_WIDTH - 1;
			};
			if (relPos.x == 0) if (auto n = getChunk(chunkPos + V3i {-1,0,0}); n)  updateMesh(n);
			if (relPos.y == 0) if (auto n = getChunk(chunkPos + V3i {0,-1,0}); n)  updateMesh(n);
			if (relPos.z == 0) if (auto n = getChunk(chunkPos + V3i {0,0,-1}); n)  updateMesh(n);
			if (relPos.x == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {1,0,0}); n)  updateMesh(n);
			if (relPos.y == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {0,1,0}); n)  updateMesh(n);
			if (relPos.z == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {0,0,1}); n)  updateMesh(n);
			return true;
		}
		return false;
	}
	bool setBlock(V3 pos, BlockID blk, bool immediate = false) {
		return setBlock((V3i)pos, blk);
	}
	std::optional<BlockID> getBlock(V3i pos) {
		if (auto c = getChunkFromBlock(pos); c)
			return c->getBlock(frac(pos, CHUNK_WIDTH));
		return {};
	}
	bool canWalkInto(V3i pos) {
		if (auto c = getChunkFromBlock(pos); c)
			return !isPhysical(c->getBlock(frac(pos, CHUNK_WIDTH)));
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
		return V3 {chunkPos * CHUNK_WIDTH} +relPos;
	}
	bool move(V3 delta) {
		relPos += delta;
		return normalize();
	}
	bool normalize() {
		auto oldChunk = chunkPos;
		bool chunkChanged = false;
		if (relPos.x >= CHUNK_WIDTH) { chunkPos.x += int(relPos.x / CHUNK_WIDTH); relPos.x = fmodf(relPos.x, CHUNK_WIDTH); chunkChanged = true; }
		if (relPos.y >= CHUNK_WIDTH) { chunkPos.y += int(relPos.y / CHUNK_WIDTH); relPos.y = fmodf(relPos.y, CHUNK_WIDTH); chunkChanged = true; }
		if (relPos.z >= CHUNK_WIDTH) { chunkPos.z += int(relPos.z / CHUNK_WIDTH); relPos.z = fmodf(relPos.z, CHUNK_WIDTH); chunkChanged = true; }
		if (relPos.x < 0) { relPos.x = fabsf(relPos.x); chunkPos.x -= int(relPos.x / CHUNK_WIDTH) + 1; relPos.x = -fmodf(relPos.x, CHUNK_WIDTH) + CHUNK_WIDTH; chunkChanged = true; }
		if (relPos.y < 0) { relPos.y = fabsf(relPos.y); chunkPos.y -= int(relPos.y / CHUNK_WIDTH) + 1; relPos.y = -fmodf(relPos.y, CHUNK_WIDTH) + CHUNK_WIDTH; chunkChanged = true; }
		if (relPos.z < 0) { relPos.z = fabsf(relPos.z); chunkPos.z -= int(relPos.z / CHUNK_WIDTH) + 1; relPos.z = -fmodf(relPos.z, CHUNK_WIDTH) + CHUNK_WIDTH; chunkChanged = true; }
		return chunkChanged;
	}
	V3 relPos;
	V3i chunkPos;
};
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	_set_invalid_parameter_handler([](const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned line, uintptr_t) {
		wprintf(L"Expression: %s\nFunction: %s\nFile: %s\nLine: %u\n", expression, function, file, line);
		assert(0);
	});
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	//for (u32 i=0; i < 256; ++i) {
	//	printf("%u\n", randomU8((u8)i));
	//}
#if 1
	{
		f32 test[4] {
			0,1,2,3
		};
		assert(linearSample(test, 0) == 0);
		assert(linearSample(test, 0.125f) == 0.5f);
		assert(linearSample(test, 0.25f) == 1);
		assert(linearSample(test, 0.375f) == 1.5f);
		assert(linearSample(test, 0.5f) == 2);
		assert(linearSample(test, 0.625f) == 2.5);
		assert(linearSample(test, 0.75f) == 3);
		assert(linearSample(test, 0.875f) == 1.5f);
		assert(linearSample(test, 1) == 0);
	}
#endif
#if 0
	for (int x = 0; x < CHUNK_WIDTH * 2; ++x) for (int y = 0; y < CHUNK_WIDTH * 2; ++y) for (int z = 0; z < CHUNK_WIDTH * 2; ++z) {
		auto c = chunkPosFromBlock(V3i{x,y,z});
		printf("%i %i %i => %i %i %i\n", x, y, z, c.x, c.y, c.z);
	}
	assert(0);
#endif
#if 0
	{
		f32 sum = 0;
		auto beg = WH::getPerformanceCounter();
		for (int i=0; i < 30000000;++i) {
			auto x = (f32)i;
			sum += voronoi(V2 {x, 0});
			sum += voronoi(V2 {x, 1});
			sum += voronoi(V2 {x, 2});
			sum += voronoi(V2 {x, 3});
			sum += voronoi(V2 {x, 4});
			sum += voronoi(V2 {x, 5});
			sum += voronoi(V2 {x, 6});
			sum += voronoi(V2 {x, 7});
			sum += voronoi(V2 {x, 8});
			sum += voronoi(V2 {x, 9});
		}
		auto end = WH::getPerformanceCounter();
		printf("sum: %f, time: %.3fms\n", sum, (f32)(end-beg)/WH::getPerformanceFrequency() * 1000);
	}
#endif
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
	spawnPos.setWorld({0, 225, 0});

	Position playerPos = spawnPos;
	V3 playerVel;
	V3 playerDimH = {0.375f, 0.875f, 0.375f};
	V3 cameraRot;

#ifdef BUILD_RELEASE
#define DEFAULT_DRAW_DISTANCE 8
#else
#define DEFAULT_DRAW_DISTANCE 4
#endif

	int chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
	int superSampleWidth = 1;

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
		x *= CHUNK_WIDTH * sizeof(BlockID);
		size_t memoryNeeded = x * x * x;
		if (memoryNeeded > memoryStatus.ullTotalPhys) {
			auto [value,unit] = normalizeBytes<double>(memoryNeeded);
			printf("Не хватает памяти (%.3f %s необходимо). Ставлю " STRINGIZE(DEFAULT_DRAW_DISTANCE) "\n", value, unit);
			chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
		}
		puts("Сглаживание [1,4]\n\t1 - Нет\n\t2 - 4x\n\t3 - 9x\n\t4 - 16x");
		std::cin >> superSampleWidth;
		if (superSampleWidth < 1 || superSampleWidth > 4) {
			superSampleWidth = 1;
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

	Renderer renderer(window.hwnd, window.clientSize * superSampleWidth);
	renderer.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11RenderTargetView* backBuffer = 0;
	ID3D11DepthStencilView* depthView = 0;
	ID3D11Texture2D* depthTex = 0;

	ID3D11VertexShader* chunkVS = renderer.createVertexShader(LDATA "shaders/chunk.hlsl");
	ID3D11PixelShader*  chunkPS = renderer.createPixelShader (LDATA "shaders/chunk.hlsl");

	ID3D11VertexShader* blockVS = renderer.createVertexShader(LDATA "shaders/block.hlsl");
	ID3D11PixelShader*  blockPS = renderer.createPixelShader (LDATA "shaders/block.hlsl");

	ID3D11VertexShader* skyVS = renderer.createVertexShader(LDATA "shaders/sky.hlsl");
	ID3D11PixelShader*  skyPS = renderer.createPixelShader (LDATA "shaders/sky.hlsl");

	ID3D11VertexShader* sunVS = renderer.createVertexShader(LDATA "shaders/sun.hlsl");
	ID3D11PixelShader*  sunPS = renderer.createPixelShader (LDATA "shaders/sun.hlsl");

	ID3D11VertexShader* blitVS = 0;
	ID3D11PixelShader*  blitPS = 0;
	{
		char buf[2] {};
		buf[0] = (char)('0' + superSampleWidth);
		D3D_SHADER_MACRO defines[2] {
			{"BLIT_SS", buf},
			{}
		};
		blitVS = renderer.createVertexShader(LDATA "shaders/blit.hlsl", defines);
		blitPS = renderer.createPixelShader (LDATA "shaders/blit.hlsl", defines);
	}

	f32 farClipPlane = 1000;

	CBuffer<0, DrawCBuffer>   drawCBuffer  {renderer};
	CBuffer<1, FrameCBuffer>  frameCBuffer {renderer};
	CBuffer<2, SceneCBuffer>  sceneCBuffer {renderer};
	CBuffer<3, ScreenCBuffer> screenCBuffer{renderer};

	sceneCBuffer.fogDistance = (f32)chunkDrawDistance * CHUNK_WIDTH;

	drawCBuffer.createDynamic();
	drawCBuffer.bindV();
	drawCBuffer.bindP();
	frameCBuffer.createDynamic();
	frameCBuffer.bindV();
	frameCBuffer.bindP();
	sceneCBuffer.createImmutable();
	sceneCBuffer.bindV();
	sceneCBuffer.bindP();
	screenCBuffer.createDynamic();
	screenCBuffer.bindV();
	screenCBuffer.bindP();

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

	{
		const V3 positions[8]{
			{ 0.5, 0.5, 0.5},
			{ 0.5, 0.5,-0.5},
			{ 0.5,-0.5, 0.5},
			{ 0.5,-0.5,-0.5},
			{-0.5, 0.5, 0.5},
			{-0.5, 0.5,-0.5},
			{-0.5,-0.5, 0.5},
			{-0.5,-0.5,-0.5},
		};
		blockMesh.load(renderer, DATA "mesh/block.mesh",
					   vertsFromFaces({{positions[5], V2{0,1}}, {positions[1], V2{1,1}},// -z
									   {positions[7], V2{0,0}}, {positions[3], V2{1,0}},
									   {positions[0], V2{0,1}}, {positions[4], V2{1,1}},// +z
									   {positions[2], V2{0,0}}, {positions[6], V2{1,0}},
									   {positions[7], V2{0,1}}, {positions[3], V2{1,1}},// -y
									   {positions[6], V2{0,0}}, {positions[2], V2{1,0}},
									   {positions[4], V2{0,1}}, {positions[0], V2{1,1}},// +y
									   {positions[5], V2{0,0}}, {positions[1], V2{1,0}},
									   {positions[4], V2{0,1}}, {positions[5], V2{1,1}},// -x
									   {positions[6], V2{0,0}}, {positions[7], V2{1,0}},
									   {positions[1], V2{0,1}}, {positions[0], V2{1,1}},// +x
									   {positions[3], V2{0,0}}, {positions[2], V2{1,0}},
									  }));
	}
	ID3D11ShaderResourceView* atlasTex       = renderer.createTexture(DATA "textures/atlas.png");
	ID3D11ShaderResourceView* atlasNormalTex = renderer.createTexture(DATA "textures/atlas_normal.png");
	ID3D11ShaderResourceView* selectionTex   = renderer.createTexture(DATA "textures/selection.png");

	ID3D11SamplerState* samplerState = renderer.createSamplerState(D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, 3);
	renderer.deviceContext->PSSetSamplers(0, 1, &samplerState);

	ID3D11BlendState* alphaBlendPreserve = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
																	 D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ONE);
	ID3D11BlendState* alphaBlendOverwrite = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
																	  D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO);
	ID3D11BlendState* alphaBlendInvertedOverwrite = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA,
																			  D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO);
	ID3D11BlendState* sunBlend = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_ALPHA,
														   D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ONE);

	RenderTarget renderTargets[2];

	f32 blendFactor[4] {1,1,1,1};

#define PATH_SAVE DATA "world.save"

	World world(renderer, playerPos.chunkPos, chunkDrawDistance);

#if 0
	for (int x=-5; x <= 5; ++x) for (int y=-5; y <= 5; ++y) for (int z=-5; z <= 5; ++z) {
		BorisHash::add({{x,y,z}, FilePos(x * 11 * 11 + y * 11 + z)});
	}
	for (int x=-5; x <= 5; ++x) for (int y=-5; y <= 5; ++y) for (int z=-5; z <= 5; ++z) {
		assert(BorisHash::find({x,y,z}) == x * 11 * 11 + y * 11 + z);
	}
	assert(0);
#endif

	auto loadWorld = [&]() {
		//TIMED_SCOPE("LOAD WORLD");
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

#define BOT 0

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

		static f32 time = 0;
		time += targetFrameTime;

		stepLerp = max(stepLerp - targetFrameTime * 10, 0.0f);

		if (input.keyDown('V'))
			moveMode = (MoveMode)(((int)moveMode + 1) % (int)MoveMode::count);

		cameraRot.x -= input.mouseDelta().y * 0.0005f;
		cameraRot.y += input.mouseDelta().x * 0.0005f;


		V3 mov;
#if BOT
		cameraRot.y += cosNoise(time * 0.1f) * 0.01f;
		++mov.z;
#endif
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

		if (input.keyHeld(VK_SHIFT) || BOT)
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
						auto opt = world.getBlock(worldTestBlock);
						if ((opt.has_value() && predicate(*opt)) || !opt.has_value()) {
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
				if (raycast(begin, end, hit, relHitBlock, [&](const BlockID& b) { return !isTransparent(b); }, playerDimH)) {
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
		auto matrixCamRotProj = projection * M4::rotationYXZ(cameraRot);
		auto matrixVP = matrixCamRotProj * M4::translation(-cameraPos);

		float dayTime = time / (60 * 1);
		
		static constexpr V3 skyColors[] {
			{.1,.1,.2},
			{.1,.1,.2},
			{.7,.4,.3},
			{.4,.7, 1},
			{.4,.7, 1},
			{.4,.7, 1},
			{.7,.4,.3},
			{.1,.1,.2},
		};
		static constexpr V3 sunColors[] {
			{0, 0, 0},
			{0, 0, 0},
			{1.5,.75, 0},
			{1, 1, 1},
			{1, 1, 1},
			{1, 1, 1},
			{1.5,.75, 0},
			{0, 0, 0},
		};
		frameCBuffer.camPos = cameraPos;
		frameCBuffer.time = time;
		frameCBuffer.lightDir = M4::rotationX(DEG2RAD(-30)) * V3 { sin(dayTime * PI2),-cos(dayTime * PI2),0};
		frameCBuffer.sunColor = linearSample(sunColors, dayTime);
		frameCBuffer.skyColor = linearSample(skyColors, dayTime);
		frameCBuffer.earthBloomMult = max(abs(frac(dayTime + 0.5f) - 0.5f) * 4 - 1, 0.0f);
		frameCBuffer.sunBloomMult = max(abs(frac(dayTime * 2 + 0.5f) - 0.5f) * 4 - 1, 0.0f);
		frameCBuffer.update();

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
			desc.Width = window.clientSize.x * superSampleWidth;
			desc.Height = window.clientSize.y * superSampleWidth;
			desc.MipLevels = 1;
			desc.SampleDesc = {1, 0};
			DHR(renderer.device->CreateTexture2D(&desc, 0, &depthTex));
			DHR(renderer.device->CreateDepthStencilView(depthTex, 0, &depthView));

			projection = M4::projection((f32)window.clientSize.x / window.clientSize.y, DEG2RAD(75), 0.01f, farClipPlane);

			for (auto& rt : renderTargets)
				rt = renderer.createRenderTarget(window.clientSize * superSampleWidth);

			f32 ssOff = 0.0f;
			switch (superSampleWidth) {
				case 2: ssOff = 0.25f; break;
				case 3: ssOff = 1.0f / 3.0f; break;
				case 4: ssOff = 0.125f; break;
			}
			auto invScreenSize = V2 {1} / V2 {window.clientSize};
			screenCBuffer.invScreenSize = invScreenSize / (f32)superSampleWidth;
			screenCBuffer.sampleOffset = invScreenSize * ssOff;
			screenCBuffer.update();
		}

		struct FrustumPlanes {
			V4 frustumPlanes[6];
			FrustumPlanes(const M4& vp) noexcept {
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
		static std::vector<Chunk*> chunksToDraw;
		chunksToDraw.clear();
		for (auto& [pos,c] : world.loadedChunks) {
			if (frustumPlanes.containsSphere(V3 {(c->position - playerPos.chunkPos) * CHUNK_WIDTH + CHUNK_WIDTH / 2}, CHUNK_WIDTH * ROOT3)) {
				chunksToDraw.push_back(c);
			}
		}

		renderer.deviceContext->ClearRenderTargetView(backBuffer, V4 {1}.data());
		renderer.deviceContext->ClearRenderTargetView(renderTargets[0].rt, V4 {1}.data());
		renderer.deviceContext->ClearRenderTargetView(renderTargets[1].rt, V4 {1}.data());
		renderer.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		renderer.deviceContext->OMSetRenderTargets(1, &renderTargets[0].rt, depthView);
		renderer.setViewport(window.clientSize* superSampleWidth);

		renderer.deviceContext->PSSetShaderResources(2, 1, &atlasTex);
		renderer.deviceContext->PSSetShaderResources(3, 1, &atlasNormalTex);
#if 0
		renderer.deviceContext->VSSetShader(approxVS, 0, 0);
		renderer.deviceContext->PSSetShader(approxPS, 0, 0);
		for (auto& c : chunksToDraw) {
			c->drawApprox(drawCBuffer);
		}
		renderer.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
#endif
		renderer.deviceContext->VSSetShader(chunkVS, 0, 0);
		renderer.deviceContext->PSSetShader(chunkPS, 0, 0);
		for (auto& c : chunksToDraw) {
			c->draw(drawCBuffer, playerPos.chunkPos, matrixVP);
		}

		{
			V3i placePos, breakPos;
			Hit hit;
			if (raycast(cameraPos, cameraPos + viewDir * 3, hit, placePos, [](const BlockID& b) {return b != BLOCK_AIR; })) {
				breakPos = placePos;
				placePos += (V3i)hit.n;
				drawCBuffer.model = M4::translation((V3)breakPos) * M4::scaling(1.01f);
				drawCBuffer.mvp = matrixVP * drawCBuffer.model;
				drawCBuffer.solidColor = {0,0,0,0.5};
				drawCBuffer.update();
				renderer.deviceContext->VSSetShader(blockVS, 0, 0);
				renderer.deviceContext->PSSetShader(blockPS, 0, 0);
				renderer.deviceContext->PSSetShaderResources(2, 1, &selectionTex);
				//renderer.deviceContext->OMSetBlendState(alphaBlendOverwrite, blendFactor, 0xFFFFFFFF);
				blockMesh.draw(renderer);
				//renderer.deviceContext->OMSetBlendState(0, blendFactor, 0xFFFFFFFF);
				if (input.mouseDown(0)) {
					if (world.setBlock(breakPos + playerPos.chunkPos * CHUNK_WIDTH, {BLOCK_AIR}, true))
						puts("Destroyed!");
				}
				if (input.mouseDown(1)) {
					if (!isInsideBlock(playerPos.relPos, playerDimH, placePos) && world.setBlock(placePos + playerPos.chunkPos * CHUNK_WIDTH, {toolBlock}, true))
						puts("Placed!");
				}
			}
		}
		drawCBuffer.mvp = matrixCamRotProj;
		drawCBuffer.solidColor = 1;
		drawCBuffer.update();

		// SUN
		renderer.deviceContext->VSSetShader(sunVS, 0, 0);
		renderer.deviceContext->PSSetShader(sunPS, 0, 0);
		renderer.draw(36);

		renderer.deviceContext->OMSetRenderTargets(1, &renderTargets[1].rt, 0);
		renderer.deviceContext->PSSetShaderResources(0, 1, &renderTargets[0].sr);

		// SKY
		renderer.deviceContext->VSSetShader(skyVS, 0, 0);
		renderer.deviceContext->PSSetShader(skyPS, 0, 0);
		renderer.draw(36);

		renderer.setViewport(window.clientSize);
		renderer.deviceContext->OMSetRenderTargets(1, &backBuffer, 0);
		
		// BLIT
		renderer.setViewport(window.clientSize);
		renderer.deviceContext->VSSetShader(blitVS, 0, 0);
		renderer.deviceContext->PSSetShader(blitPS, 0, 0);
		renderer.deviceContext->PSSetShaderResources(0, 1, &renderTargets[1].sr);
		//renderer.deviceContext->OMSetBlendState(alphaBlendInvertedOverwrite, blendFactor, 0xFFFFFFFF);
		renderer.draw(3);
		//renderer.deviceContext->OMSetBlendState(0, blendFactor, 0xFFFFFFFF);

		ID3D11ShaderResourceView* null = 0;
		renderer.deviceContext->PSSetShaderResources(0, 1, &null);

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
		static u32 meshesBuiltInSec = 0;
		static f32 generateMS = 0;
		static f32 buildMeshMS = 0;
		static f32 secondTimer = 0;

		secondTimer += targetFrameTime;
		if (secondTimer >= 1) {
			secondTimer -= 1;
			generateMS = (f32)generateTime / generateCount / counterFrequency * 1000.f;
			buildMeshMS = (f32)buildMeshTime / buildMeshCount / counterFrequency * 1000.f;
			meshesBuiltInSec = meshesBuilt.exchange(0);
		}
		auto ram = ramUsage.load();
		auto [ramValue, ramUnit] = normalizeBytes<double>(ram);
		auto [vramValue, vramUnit] = normalizeBytes<double>(vramUsage.load());
		debugGenerateMutex.lock();
		debugBuildMeshMutex.lock();
		sprintf(windowTitle, "gstorm - Position: (%i/%i/%i) (%.2f/%.2f/%.2f), RAM usage: %.3f %s, VRAM usage: %.3f %s, Draws: %u/f, Gen. time: %.3fms/c, Mesh time: %.3fms/c, Loaded chunks: %zu, Meshes built: %u/s", 
				playerPos.chunkPos.x, playerPos.chunkPos.y, playerPos.chunkPos.z,
				playerPos.relPos.x, playerPos.relPos.y, playerPos.relPos.z,
				ramValue, ramUnit, vramValue, vramUnit, renderer.getDrawCalls(), generateMS, buildMeshMS, ram / (CHUNK_SIZE + sizeof(Chunk)), meshesBuiltInSec);
		debugBuildMeshMutex.unlock();
		debugGenerateMutex.unlock();
		SetWindowText(window.hwnd, windowTitle);
	}
	world.loadQueueNoPush = true;
	ClipCursor(0);
	SetCursor(LoadCursorA(0, IDC_ARROW));
	DestroyWindow(window.hwnd);

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