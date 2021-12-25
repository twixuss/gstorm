// TODO:
// Free memory of far chunks, but left them visible
// Make approximate meshes for far chunks

//#pragma warning(disable:4189)//unused
#pragma warning(disable:4324) // align zero padding
#pragma warning(disable:4359) // align bigger
#pragma warning(disable:4201) // no struct name
#include "common.h"
#include "math.h"
#include "math.cpp"
#include "winhelper.h"
#include "d3d11helper.h"

#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>

#include <direct.h>
#include <io.h>
#include <Psapi.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <timeapi.h>
#include <Windows.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_VISIBLE)
#define DATA "../data/"
#define LDATA L"../data/"
#define FILEPOS_INVALID ((FilePos)-1)

using FilePos = u64;

struct SaveFile {
	nd FILE* init() ne {
		const char* path = DATA "save/world";
		FILE* file = fopen(path, "r+b");
		if (file) {
			fread(this, sizeof(*this), 1, file);
		}
		else {
			file = fopen(path, "w+b");
			assert(file);
			fwrite(this, sizeof(*this), 1, file);
		}
		return file;
	}
	nd FilePos add(FILE* file, V3i key) ne {
		DEFER {write(file); };
		auto& bucket = buckets[makeHash(key)];
		FilePos& firstBlockPos = bucket.firstBlockPos;
		FilePos& lastBlockPos = bucket.lastBlockPos;

		auto duplicate = find(file, key);
		assert(duplicate == FILEPOS_INVALID);
		if (firstBlockPos == 0) {
			firstBlockPos = lastBlockPos = getFileSize(file);
			Block newBlock = {};
			auto& entry = newBlock.entries[0];
			entry.key = key;
			entry.filePos = firstBlockPos + sizeof(Block);
			writeBlock(file, newBlock, firstBlockPos);
			return entry.filePos;
		}
		else {
			Block lastBlock = readBlock(file, lastBlockPos);
			int targetIdx = 1;
			for (; targetIdx < entriesPerBlock; targetIdx++) {
				auto& entry = lastBlock.entries[targetIdx];
				if (entry.invalid()) {
					entry.key = key;
					entry.filePos = getFileSize(file);
					writeBlock(file, lastBlock, lastBlockPos);
					return entry.filePos;
				}
			}
			auto prelastBlockPos = lastBlockPos;
			auto prelastBlock = readBlock(file, prelastBlockPos);
			prelastBlock.nextBlock = getFileSize(file);

			lastBlockPos = prelastBlock.nextBlock;
			lastBlock = {};
			auto& entry = lastBlock.entries[0];
			entry.key = key;
			entry.filePos = lastBlockPos + sizeof(Block);

			writeBlock(file, prelastBlock, prelastBlockPos);
			writeBlock(file, lastBlock, lastBlockPos);
			return entry.filePos;
		}
	}
	nd FilePos find(FILE* file, V3i key) {
		FilePos currentBlockPos = buckets[makeHash(key)].firstBlockPos;
		while (currentBlockPos) {
			auto currentBlock = readBlock(file, currentBlockPos);
			for (int i = 0; i < entriesPerBlock; i++) {
				if (currentBlock.entries[i].invalid())
					break;
				if (currentBlock.entries[i].key == key)
					return currentBlock.entries[i].filePos;
			}
			currentBlockPos = currentBlock.nextBlock;
		}
		return FILEPOS_INVALID;
	}
private:
	static constexpr u32 bucketCount = 65536;
	static constexpr u32 entriesPerBlock = 16;

#pragma pack(push, 1)
	struct Entry {
		V3i key {0,0,0};
		FilePos filePos = FILEPOS_INVALID;
		nd bool invalid() {
			return filePos == FILEPOS_INVALID;
		}
	};
#pragma pack(pop)
	struct Block {
		FilePos nextBlock = 0;
		Entry entries[entriesPerBlock] {};
	};
	struct Bucket {
		FilePos firstBlockPos = 0, lastBlockPos = 0;
	};

	Bucket buckets[bucketCount];

	void write(FILE* file) {
		_fseeki64(file, 0, SEEK_SET);
		assert(fwrite(this, sizeof(*this), 1, file) != 0);
	}
	void debug(FILE* file) {
		for (u32 i=0; i < bucketCount; ++i) {
			auto& b = buckets[i];
			printf("Bucket # %u, firstBlock: %zu, lastBlock: %zu\n", i, b.firstBlockPos, b.lastBlockPos);
		}
		for (int j=0; j < bucketCount; ++j) {
			FilePos currentBlockPos = buckets[j].firstBlockPos;
			while (currentBlockPos) {
				auto currentBlock = readBlock(file, currentBlockPos);
				printf("position in file: %llu\nnextBlock: %llu\n", currentBlockPos, currentBlock.nextBlock);
				for (int i = 0; i < entriesPerBlock; i++) {
					auto& e = currentBlock.entries[i];
					if (e.invalid())
						break;
					printf("w: ");
					std::cout << e.key;
					printf(", f: %llu\n", e.filePos);
				}
				puts("");
				currentBlockPos = currentBlock.nextBlock;
			}
		}
	}
	nd u32 makeHash(V3i key) {
		return std::hash<V3i>{}(key) % bucketCount;
	}
	nd Block readBlock(FILE* file, FilePos position) {
		_fseeki64(file, position, SEEK_SET);
		Block result;
		assert(fread(&result, sizeof(Block), 1, file) != 0);
		return result;
	}
	void writeBlock(FILE* file, Block& blk, FilePos position) {
		_fseeki64(file, position, SEEK_SET);
		assert(fwrite(&blk, sizeof(Block), 1, file) != 0);
	}
	nd FilePos getFileSize(FILE* file) {
		_fseeki64(file, 0, SEEK_END);
		return _ftelli64(file);
	}
};
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
		previous = current;
		current.mouseDelta = {};
	}
	nd bool keyHeld(u8 k) { return current.keys[k]; }
	nd bool keyDown(u8 k) { return current.keys[k] && !previous.keys[k]; }
	nd bool keyUp(u8 k) { return !current.keys[k] && previous.keys[k]; }
	nd bool mouseHeld(u8 k) { return current.mouse[k]; }
	nd bool mouseDown(u8 k) { return current.mouse[k] && !previous.mouse[k]; }
	nd bool mouseUp(u8 k) { return !current.mouse[k] && previous.mouse[k]; }
	nd auto mousePosition() { return current.mousePosition; }
	nd auto mouseDelta() { return current.mouseDelta; }
};
struct Window {
	HWND hwnd = 0;
	V2i clientSize;
	bool running = true;
	bool resize = true;
	bool killFocus = false;
};
template<class T>
struct ConcurrentQueue {
	void push(T chunk) {
		std::unique_lock l(mutex);
		buffer.push_back(chunk);
	}
	std::optional<T> pop() {
		std::unique_lock l(mutex);
		if (!buffer.size()) {
			return {};
		}
		DEFER {
			buffer.pop_front();
		};
		return buffer.front();
	}
	template<class Callback>
	void pop_all(Callback&& callback) {
		if (!buffer.size())
			return;
		mutex.lock();
		toProcess.resize(buffer.size());
		std::copy(begin(buffer), end(buffer), toProcess.begin());
		buffer.clear();
		mutex.unlock();
		std::for_each(begin(toProcess), end(toProcess), callback);
	}
private:
	std::vector<T> toProcess;
	std::deque<T> buffer;
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
		_default,
		x,
		topSideBottom,
		//water
	};
	i16 atlasPos = 0;
	i16 atlasAxisOffsets[6] {};
	Type type {};
	bool randomizeUv[6] {};
	nd i16 offsetAtlasPos(u8 axis) {
		return atlasPos + atlasAxisOffsets[axis];
	}
};
std::unordered_map<BlockID, BlockInfo> blockInfos;

#define BLOCK_AIR        ((BlockID)0)
#define BLOCK_DIRT       ((BlockID)1)
#define BLOCK_GRASS      ((BlockID)2)
#define BLOCK_TALL_GRASS ((BlockID)3)
//#define BLOCK_WATER      ((BlockID)4)

bool isTransparent(BlockID id) {
	switch (id) {
		case BLOCK_AIR:
		case BLOCK_TALL_GRASS:
		//case BLOCK_WATER:
			return true;
	}
	return false;
}
bool isPhysical(BlockID id) {
	switch (id) {
		case BLOCK_AIR:
		case BLOCK_TALL_GRASS:
		//case BLOCK_WATER:
			return false;
	}
	return true;
}
bool isBreakable(BlockID id) {
	switch (id) {
		case BLOCK_AIR:
		//case BLOCK_WATER:
			return false;
	}
	return true;
}

#define CHUNK_WIDTH 64
#define CHUNK_VOLUME (CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH)
#define FOR_BLOCK_IN_CHUNK 		    \
for (int z=0; z < CHUNK_WIDTH; ++z) \
for (int x=0; x < CHUNK_WIDTH; ++x) \
for (int y=0; y < CHUNK_WIDTH; ++y)
#define BLOCK_INDEX(x,y,z) ((z) * CHUNK_WIDTH * CHUNK_WIDTH + (x) * CHUNK_WIDTH + (y))

#define FOREACH_BLOCK(b, blocks) FOR_BLOCK_IN_CHUNK \
for (int i=0;i<CHUNK_VOLUME;++i) \
if (auto& b = blocks[i];false);else

#include "../data/shaders/chunkVertex.h"
struct alignas(16) DrawCBuffer {
	M4 mvp;
	M4 model;
	M4 modelCamPos;
	M4 camRotProjection;
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
	M4 projection;
	V2 sampleOffset;
	V2 invScreenSize;
};
struct Vertex {
	V3 position;
	V2 uv;
};

struct Mesh {
	ID3D11Buffer* vBuffer = 0;
	ID3D11ShaderResourceView* vBufferView = 0;
	u32 vertexCount = 0;
	template<size_t size>
	void load(D3D11& renderer, const std::array<Vertex, size>& vertices) {
		u32 vertexSize = sizeof(Vertex);
		vertexCount = (u32)vertices.size();
		assert(vertexCount);
		u32 vertexBufferSize = vertexCount * vertexSize;

		vBuffer = renderer.createImmutableStructuredBuffer(vertexBufferSize, vertexSize, vertices.data());

		vBufferView = renderer.createBufferView(vBuffer, vertexCount);
	}
	void draw(D3D11& renderer) {
		if (!vertexCount)
			return;
		renderer.vsSetShaderResource(0, vBufferView);
		renderer.draw(vertexCount);
	}
};
Mesh blockMesh;
#define CHUNK_SIZE (sizeof(BlockID) * CHUNK_VOLUME)
#define CHUNK_FILE DATA "save/world"
void printChunk(BlockID*, V3i);
V3i chunkPosFromBlock(V3i pos) {
	return floor(pos, CHUNK_WIDTH);
}
V3i chunkPosFromBlock(V3 pos) {
	return chunkPosFromBlock(V3i {pos});
}
#include "chunk.h"
using ChunkArena = Arena<Chunk, 0x10000>;
#ifdef BUILD_RELEASE
#define HOT_DIST 4
#else
#define HOT_DIST 1
#endif
struct World {
	ChunkArena chunkArena;
	BlockArena blockArena;
	std::unordered_map<V3i, Chunk*> loadedChunks;
	ConcurrentQueue<Chunk*> loadQueue, unloadQueue;
	ConcurrentQueue<std::pair<Chunk*, Neighbors>> meshQueue;
	ChunkVertex* vertexPools[2]; // extra one for immediate mesh build
	u32* indexPools[2]; // extra one for immediate mesh build
	std::atomic_bool stopWork = false;
	std::vector<Chunk*> chunksWantedToDelete;
	D3D11& renderer;
	const V3i& playerChunk;
	const int loadDistance;
	std::thread chunkLoader, meshBuilder;
	SaveFile& saveFile;
	FILE* saveFileHandle;
	World(D3D11& renderer, const V3i& playerChunk, int loadDistance) : saveFile(*new SaveFile), renderer(renderer), playerChunk(playerChunk), loadDistance(loadDistance) {
		int w = loadDistance * 2 + 1;
		loadedChunks.reserve(w * w * w);
		for (auto& pool : vertexPools) {
			pool = (ChunkVertex*)malloc(maxChunkVertexCount * sizeof(ChunkVertex));
		}
		for (auto& pool : indexPools) {
			pool = (u32*)malloc(maxChunkIndexCount * sizeof(u32));
		}
		saveFileHandle = saveFile.init();
		chunkLoader = std::thread {[this]() {
			SetThreadName((DWORD)-1, "Chunk loader");
			while (updateChunks());
		}};
		meshBuilder = std::thread {[this]() {
			SetThreadName((DWORD)-1, "Mesh builder");
			while (buildMeshes());
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

		static std::vector<Chunk*> chunksToUnload;
		chunksToUnload.clear();
		//for (auto& [pos, c] : loadedChunks) {
		chunkArena.for_each([&] (Chunk* c) {
			auto dist = maxDistance(playerChunk, c->position);
			if (dist > loadDistance) {
				unloadQueue.push(c);
				chunksToUnload.push_back(c);
			}
		});
		//}
		for (auto& c : chunksToUnload) {
			loadedChunks.erase(c->position);
		}
	}
	void saveAndDelete(Chunk* c) {
		if (c->needToSave) {
			auto& filePos = c->filePos;
			if (filePos == FILEPOS_INVALID)
				filePos = saveFile.add(saveFileHandle, c->position);
			c->save(saveFileHandle, filePos);
		}
		c->free(blockArena);
		chunkArena.free(c);
	}
	// main thread
	void save() {
		stopWork = true;
		puts("Waiting for chunk loader thread...");
		chunkLoader.join();
		puts("Waiting for mesh builder thread...");
		meshBuilder.join();

		size_t progress = 0;
		//for (auto& [position, c] : loadedChunks) {
		chunkArena.for_each([&] (Chunk* c) {
			c->userCount = 0;
			saveAndDelete(c);
			printf("Saving world... %zu%%\r", progress++ * 100 / loadedChunks.size());
		});
		//}
		puts("Saving world... 100%");
		fclose(saveFileHandle);
	}
	Neighbors getNeighbors(Chunk* c) {
		Neighbors neighbors;
		neighbors[0] = findChunk(c->position + V3i { 1, 0, 0});
		neighbors[1] = findChunk(c->position + V3i {-1, 0, 0});
		neighbors[2] = findChunk(c->position + V3i { 0, 1, 0});
		neighbors[3] = findChunk(c->position + V3i { 0,-1, 0});
		neighbors[4] = findChunk(c->position + V3i { 0, 0, 1});
		neighbors[5] = findChunk(c->position + V3i { 0, 0,-1});
		for (auto& n : neighbors) {
			if (n && !n->blocks)
				n = 0;
		}
		return neighbors;
	}
	// chunk loader thread
	void buildMesh(Chunk* c) {
		auto neighbors = getNeighbors(c);
		for (auto n : neighbors)
			if (!n)
				return;
		for (auto n : neighbors) {
			++n->userCount;
		}
		++c->userCount;
		meshQueue.push({c, neighbors});
	}
	bool wantToDelete(Chunk* c) {
		Chunk::State expectedState = Chunk::State::loaded;
		if (c->state.compare_exchange_strong(expectedState, Chunk::State::wantedToBeDeleted)) {
			assert(!contains(chunksWantedToDelete, c));
			chunksWantedToDelete.push_back(c);
			return true;
		}
		return false;
	}
	bool loadChunks() {
		loadQueue.pop_all([this](Chunk* c) {
			if (stopWork)
				return;

			DEFER {
				--c->userCount;
			};

			if (maxDistance(c->position, playerChunk) > loadDistance) {
				wantToDelete(c);
				return;
			}

			auto tryLoad = [&]() {
				auto filePos = saveFile.find(saveFileHandle, c->position);
				if (filePos == FILEPOS_INVALID)
					return false;
				c->load(saveFileHandle, filePos, blockArena);
				return true;
			};

			if (!saveFileHandle || !tryLoad()) {
				c->generate(blockArena);
			}
			Chunk::State expectedState = Chunk::State::unloaded;
			assert(c->state.compare_exchange_strong(expectedState, Chunk::State::loaded));
			buildMesh(c);
			for (auto& n : getNeighbors(c)) {
				if (n) {
					buildMesh(n);
				}
			}
		});
		std::this_thread::sleep_for(std::chrono::milliseconds {1}); // TODO: Semaphore
		return !stopWork;
	}
	void unloadChunks() {
		unloadQueue.pop_all([this](Chunk* c) {
			wantToDelete(c);
		});

		for (auto& c : chunksWantedToDelete) {
			assert(c->state == Chunk::State::wantedToBeDeleted);
			//assert(c->userCount == 0);
			if (c->userCount == 0) {
				//c->workEnded.wait();
				saveAndDelete(c);
			}
		}
		chunksWantedToDelete.clear();
	}
	bool updateChunks() {
		unloadChunks();
		return loadChunks();
	}
	bool buildMeshes() {
		meshQueue.pop_all([this](std::pair<Chunk*, Neighbors> pair) {
			if (stopWork)
				return;
			auto c = pair.first;
			auto neighbors = pair.second;
			DEFER {
				--c->userCount;
				for (auto& n : neighbors) {
					--n->userCount;
				}
			};
			c->generateMesh(vertexPools[0], indexPools[0], neighbors);
		});
		std::this_thread::sleep_for(std::chrono::milliseconds {1});
		return !stopWork;
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
		chunk = chunkArena.allocate(pos, renderer).data;
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
		if (!c) {
			return false;
		}
		auto relPos = frac(worldPos, CHUNK_WIDTH);
		if (c->setBlock(relPos, blk)) {
			auto updateMesh = [this, immediate](Chunk* chunk) {
				if (immediate) {
					chunk->generateMesh(vertexPools[1], indexPools[1], getNeighbors(chunk));
				}
				else {
					buildMesh(chunk);
				}
			};
			updateMesh(c);
			if (relPos.x == 0) if (auto n = getChunk(chunkPos + V3i {-1,0,0}); n) updateMesh(n);
			if (relPos.y == 0) if (auto n = getChunk(chunkPos + V3i {0,-1,0}); n) updateMesh(n);
			if (relPos.z == 0) if (auto n = getChunk(chunkPos + V3i {0,0,-1}); n) updateMesh(n);
			if (relPos.x == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {1,0,0}); n) updateMesh(n);
			if (relPos.y == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {0,1,0}); n) updateMesh(n);
			if (relPos.z == CHUNK_WIDTH - 1) if (auto n = getChunk(chunkPos + V3i {0,0,1}); n) updateMesh(n);
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
#define MAX_CHUNK_POSITION (int(0x80000000 / CHUNK_WIDTH) - 32)
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
		if (relPos.x >= CHUNK_WIDTH) { chunkPos.x += int(relPos.x / CHUNK_WIDTH); relPos.x = fmodf(relPos.x, CHUNK_WIDTH); }
		if (relPos.y >= CHUNK_WIDTH) { chunkPos.y += int(relPos.y / CHUNK_WIDTH); relPos.y = fmodf(relPos.y, CHUNK_WIDTH); }
		if (relPos.z >= CHUNK_WIDTH) { chunkPos.z += int(relPos.z / CHUNK_WIDTH); relPos.z = fmodf(relPos.z, CHUNK_WIDTH); }
		if (relPos.x < 0) { relPos.x = -relPos.x; chunkPos.x -= int(relPos.x / CHUNK_WIDTH) + 1; relPos.x = -fmodf(relPos.x, CHUNK_WIDTH) + CHUNK_WIDTH; }
		if (relPos.y < 0) { relPos.y = -relPos.y; chunkPos.y -= int(relPos.y / CHUNK_WIDTH) + 1; relPos.y = -fmodf(relPos.y, CHUNK_WIDTH) + CHUNK_WIDTH; }
		if (relPos.z < 0) { relPos.z = -relPos.z; chunkPos.z -= int(relPos.z / CHUNK_WIDTH) + 1; relPos.z = -fmodf(relPos.z, CHUNK_WIDTH) + CHUNK_WIDTH; }
		if (chunkPos.x >  MAX_CHUNK_POSITION) { chunkPos.x =  MAX_CHUNK_POSITION; relPos.x = CHUNK_WIDTH - .001f; }
		if (chunkPos.y >  MAX_CHUNK_POSITION) { chunkPos.y =  MAX_CHUNK_POSITION; relPos.y = CHUNK_WIDTH - .001f; }
		if (chunkPos.z >  MAX_CHUNK_POSITION) { chunkPos.z =  MAX_CHUNK_POSITION; relPos.z = CHUNK_WIDTH - .001f; }
		if (chunkPos.x < -MAX_CHUNK_POSITION) { chunkPos.x = -MAX_CHUNK_POSITION; relPos.x = 0; }
		if (chunkPos.y < -MAX_CHUNK_POSITION) { chunkPos.y = -MAX_CHUNK_POSITION; relPos.y = 0; }
		if (chunkPos.z < -MAX_CHUNK_POSITION) { chunkPos.z = -MAX_CHUNK_POSITION; relPos.z = 0; }
		return chunkPos != oldChunk;
	}
	V3 relPos;
	V3i chunkPos;
};
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);
	SetConsoleOutputCP(1251);
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
	const f32 walkMaxSpeed = 15;
	const f32 jumpForce = 250;
	const f32 airMult = 0.2;
	const f32 noclipMult = 4;
	const f32 groundFriction = .5f;
	const f32 airFriction = 0.2f;
	const f32 noclipFriction = .5;
	const f32 noclipMaxSpeed = 50;
	const f32 camHeight = 0.625f;

	Position spawnPos;
	spawnPos.setWorld({0, 225, 0});

	Position playerPos = spawnPos;
	V3 playerVel;
	V3 playerDimH = {0.375f, 0.875f, 0.375f};
	V3 cameraRot;

#ifdef BUILD_RELEASE
#define DEFAULT_DRAW_DISTANCE 4
#else
#define DEFAULT_DRAW_DISTANCE 2
#endif

	int chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
	int superSampleWidth = 1;

	puts(R"(		Menu
	1 - Start
	2 - Configure)");

	char menuItem;
	std::cin >> menuItem;
	if (menuItem == '2') {
		puts("Draw distance (recommend from 2 to 8)");
		std::cin >> chunkDrawDistance;
		if (chunkDrawDistance <= 0) {
			chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
			puts("Negative number is not allowed. Changing to default " STRINGIZE(DEFAULT_DRAW_DISTANCE));
		}
		MEMORYSTATUSEX memoryStatus {};
		memoryStatus.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memoryStatus);
		size_t x = chunkDrawDistance * 2 + 1;
		x *= CHUNK_WIDTH * sizeof(BlockID);
		size_t memoryNeeded = x * x * x;
		if (memoryNeeded > memoryStatus.ullTotalPhys) {
			auto [value,unit] = normalizeBytes<double>(memoryNeeded);
			printf("Not enough memory (%.3f %s is needed). Defaulting to " STRINGIZE(DEFAULT_DRAW_DISTANCE) "\n", value, unit);
			chunkDrawDistance = DEFAULT_DRAW_DISTANCE;
		}
		puts("Antialiasing [1,4]\n\t1 - Off\n\t2 - 4x\n\t3 - 9x\n\t4 - 16x");
		std::cin >> superSampleWidth;
		if (superSampleWidth < 1 || superSampleWidth > 4) {
			superSampleWidth = 1;
			puts("Invalid input. Defaulting to 1");
		}
	}
	int chunkLoadDistance = chunkDrawDistance + 1;

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
	windowRect.setXYWH((screenWidth - (LONG)window.clientSize.x) / 2,
					   (screenHeight - (LONG)window.clientSize.y) / 2,
					   (LONG)window.clientSize.x,
					   (LONG)window.clientSize.y);

	AdjustWindowRect(&windowRect, WINDOW_STYLE, 0);

	window.hwnd = CreateWindowExA(0, wndClass.lpszClassName, "Galaxy Storm", WINDOW_STYLE,
								windowRect.left, windowRect.top, windowRect.getWidth(), windowRect.getHeight(), 0, 0, instance, &window);
	assert(window.hwnd);

	SetForegroundWindow(window.hwnd);

	RAWINPUTDEVICE RawInputMouseDevice = {};
	RawInputMouseDevice.usUsagePage = 0x01;
	RawInputMouseDevice.usUsage = 0x02;

	if (!RegisterRawInputDevices(&RawInputMouseDevice, 1, sizeof(RAWINPUTDEVICE))) {
		assert(0);
	}

	D3D11 renderer(window.hwnd);
	renderer.setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	FutureQueue<ID3D11ShaderResourceView*> textureFQ;
	FutureQueue<ID3D11VertexShader*> vShaderFQ;
	FutureQueue<ID3D11PixelShader*> pShaderFQ;

	textureFQ.push(renderer.loadTextureAsync(DATA "textures/atlas.png"));
	textureFQ.push(renderer.loadTextureAsync(DATA "textures/atlas_normal.png"));
	textureFQ.push(renderer.loadTextureAsync(DATA "textures/selection.png"));

	ID3D11VertexShader* chunkVS;
	ID3D11PixelShader* chunkPS;

	ID3D11VertexShader* blockVS;
	ID3D11PixelShader* blockPS;

	ID3D11VertexShader* skyVS;
	ID3D11PixelShader* skyPS;

	ID3D11VertexShader* sunVS;
	ID3D11PixelShader* sunPS;

	ID3D11VertexShader* blitVS;
	ID3D11PixelShader* blitPS;

	auto compileShaders = [&]() {
		vShaderFQ.push(renderer.createVertexShaderAsync(LDATA "shaders/chunk.hlsl"));
		vShaderFQ.push(renderer.createVertexShaderAsync(LDATA "shaders/block.hlsl"));
		vShaderFQ.push(renderer.createVertexShaderAsync(LDATA "shaders/sky.hlsl"));
		vShaderFQ.push(renderer.createVertexShaderAsync(LDATA "shaders/sun.hlsl"));

		pShaderFQ.push(renderer.createPixelShaderAsync(LDATA "shaders/chunk.hlsl"));
		pShaderFQ.push(renderer.createPixelShaderAsync(LDATA "shaders/block.hlsl"));
		pShaderFQ.push(renderer.createPixelShaderAsync(LDATA "shaders/sky.hlsl"));
		pShaderFQ.push(renderer.createPixelShaderAsync(LDATA "shaders/sun.hlsl"));

		{
			char buf[2] {};
			buf[0] = (char)('0' + superSampleWidth);
			std::vector<D3D_SHADER_MACRO> defines {
				{"BLIT_SS", buf},
				{}
			};
			vShaderFQ.push(renderer.createVertexShaderAsync(LDATA "shaders/blit.hlsl", defines));
			pShaderFQ.push(renderer.createPixelShaderAsync (LDATA "shaders/blit.hlsl", defines));
		}
		chunkVS = vShaderFQ.pop();
		blockVS = vShaderFQ.pop();
		skyVS   = vShaderFQ.pop();
		sunVS   = vShaderFQ.pop();

		chunkPS = pShaderFQ.pop();
		blockPS = pShaderFQ.pop();
		skyPS   = pShaderFQ.pop();
		sunPS   = pShaderFQ.pop();

		blitVS  = vShaderFQ.pop();
		blitPS  = pShaderFQ.pop();

		puts("Shaders compiled");
	};
	auto freeShaders = [&]() {
		chunkVS->Release();
		chunkPS->Release();
		blockVS->Release();
		blockPS->Release();
		skyVS  ->Release();
		skyPS  ->Release();
		sunVS  ->Release();
		sunPS  ->Release();
		blitVS ->Release();
		blitPS ->Release();
	};
	compileShaders();

	ID3D11ShaderResourceView* atlasTex       = textureFQ.pop();
	ID3D11ShaderResourceView* atlasNormalTex = textureFQ.pop();
	ID3D11ShaderResourceView* selectionTex   = textureFQ.pop();
	puts("Textures loaded");

	f32 farClipPlane = 2000;

	ID3D11RenderTargetView* backBuffer = 0;
	D3D11::DepthStencilView depthView;

	D3D11::ConstantBuffer<DrawCBuffer  > drawCBuffer  ;
	D3D11::ConstantBuffer<FrameCBuffer > frameCBuffer ;
	D3D11::ConstantBuffer<SceneCBuffer > sceneCBuffer ;
	D3D11::ConstantBuffer<ScreenCBuffer> screenCBuffer;

	sceneCBuffer.fogDistance = (f32)chunkDrawDistance * CHUNK_WIDTH;

	drawCBuffer  .create(renderer, D3D11_USAGE_DYNAMIC);
	frameCBuffer .create(renderer, D3D11_USAGE_DYNAMIC);
	sceneCBuffer .create(renderer, D3D11_USAGE_IMMUTABLE);
	screenCBuffer.create(renderer, D3D11_USAGE_DYNAMIC);

	renderer.bindConstantBuffersV(0, drawCBuffer, frameCBuffer, sceneCBuffer, screenCBuffer);
	renderer.bindConstantBuffersP(0, drawCBuffer, frameCBuffer, sceneCBuffer, screenCBuffer);

	M4 projection;

	blockInfos[BLOCK_DIRT] = {2, {}, BlockInfo::Type::_default, {1,1,1,1,1,1}};
	blockInfos[BLOCK_GRASS] = {0, {1,1,0,2,1,1}, BlockInfo::Type::topSideBottom, {0,0,1,1,0,0}};
	blockInfos[BLOCK_TALL_GRASS] = {3, {}, BlockInfo::Type::x};
	//blockInfos[BLOCK_WATER] = {4, {}, BlockInfo::Type::water};

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
		blockMesh.load(renderer, triangulateQuads(std::array<Vertex, 24> {
			Vertex{positions[5], V2{0,1}}, {positions[1], V2{1,1}},// -z
			Vertex{positions[7], V2{0,0}}, {positions[3], V2{1,0}},
			Vertex{positions[0], V2{0,1}}, {positions[4], V2{1,1}},// +z
			Vertex{positions[2], V2{0,0}}, {positions[6], V2{1,0}},
			Vertex{positions[7], V2{0,1}}, {positions[3], V2{1,1}},// -y
			Vertex{positions[6], V2{0,0}}, {positions[2], V2{1,0}},
			Vertex{positions[4], V2{0,1}}, {positions[0], V2{1,1}},// +y
			Vertex{positions[5], V2{0,0}}, {positions[1], V2{1,0}},
			Vertex{positions[4], V2{0,1}}, {positions[5], V2{1,1}},// -x
			Vertex{positions[6], V2{0,0}}, {positions[7], V2{1,0}},
			Vertex{positions[1], V2{0,1}}, {positions[0], V2{1,1}},// +x
			Vertex{positions[3], V2{0,0}}, {positions[2], V2{1,0}},
		}));
	}
	ID3D11SamplerState* pointSamplerState = renderer.createSamplerState(D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, 3);
	ID3D11SamplerState* linearSamplerState = renderer.createSamplerState(D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_MIN_MAG_MIP_LINEAR, 3);
	renderer.psSetSampler(0, pointSamplerState);
	renderer.psSetSampler(1, linearSamplerState);

	//ID3D11BlendState* alphaBlendPreserve = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
	//																 D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ONE);
	//ID3D11BlendState* alphaBlendOverwrite = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
	//																  D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO);
	//ID3D11BlendState* alphaBlendInvertedOverwrite = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA,
	//																		  D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO);
	//ID3D11BlendState* sunBlend = renderer.createBlendState(D3D11_BLEND_OP_ADD, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_ALPHA,
	//													   D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ONE);

	D3D11::RenderTarget renderTargets[2];

	f32 blendFactor[4] {1,1,1,1};

#define PATH_SAVE DATA "world.save"

	World world(renderer, playerPos.chunkPos, chunkLoadDistance);

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
		int loadDist = chunkLoadDistance; // +1 because mesh will generate when all neighbors are available
		//world.loadQueue.mutex.lock();
#if 0
		for (int x = loadDist; x <= loadDist; ++x) {
			for (int y = loadDist; y <= loadDist; ++y) {
				for (int z = loadDist; z <= loadDist; ++z) {
					world.seeChunk(playerChunk + V3i {x,y,z});
				}
			}
		}
#else
		for (int r = 0; r <= loadDist; ++r) {
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

	char windowTitle[512] {};

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

					//bool extended   =  (msg.lParam & u32(1 << 24)) != 0;
					bool context    =  (msg.lParam & u32(1 << 29)) != 0;
					bool previous   =  (msg.lParam & u32(1 << 30)) != 0;
					bool transition =  (msg.lParam & u32(1 << 31)) != 0;
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

		if (input.keyHeld(VK_SHIFT) || BOT)
			accelMult *= 2;

		auto maxSpeed = moveMode == MoveMode::walk ? walkMaxSpeed : noclipMaxSpeed;
		playerVel += mov * accelMult * ((maxSpeed - dot(playerVel, mov)) / maxSpeed);
		playerVel += acceleration * targetFrameTime * 2;

		if (moveMode == MoveMode::walk) {
			V2 v {playerVel.x, playerVel.z};
			auto ls = v.lengthSqr();
			if (ls > 0) {
				auto l = sqrtf(ls);
				v = (v / l) * max(l - (grounded ? groundFriction : airFriction), 0.f);
				playerVel.x = v.x;
				playerVel.z = v.y;
			}
		}
		else {
			auto ls = playerVel.lengthSqr();
			if (ls > 0) {
				auto l = sqrtf(ls);
				playerVel = (playerVel / l) * max(l - groundFriction, 0.f);
			}
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
			playerVel = 0;
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

		if (input.keyDown('R')) {
			freeShaders();
			compileShaders();
		}

		auto cameraPos = playerPos.relPos;
		cameraPos.y += camHeight;
		cameraPos.y -= stepLerp;
		V3 viewDir = M4::rotationZXY(-cameraRot) * V3 { 0, 0, 1 };

		const f32 dayPeriod = 4000; // minutes
		const f32 startHours = 7;
		static f32 time = dayPeriod * 60 * (startHours / 24);
		time += targetFrameTime;

		float dayTime = time / (60 * dayPeriod);

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
			{1.5,.75,.1},
			{1, 1, 1},
			{1, 1, 1},
			{1, 1, 1},
			{1.5,.75,.1},
			{0, 0, 0},
		};
		frameCBuffer.camPos = cameraPos;
		frameCBuffer.time = time;
		frameCBuffer.lightDir = M4::rotationX(DEG2RAD(-30)) * V3 { sin(dayTime * PI2),-cos(dayTime * PI2),0};
		frameCBuffer.sunColor = linearSample(sunColors, dayTime);
		frameCBuffer.skyColor = linearSample(skyColors, dayTime);
		frameCBuffer.earthBloomMult = max(abs(frac(dayTime + 0.5f) - 0.5f) * 4 - 1, 0.0f);
		frameCBuffer.sunBloomMult = max(abs(frac(dayTime * 2 + 0.5f) - 0.5f) * 4 - 1, 0.0f);
		frameCBuffer.update(renderer);

		if (window.resize) {
			window.resize = false;

			depthView.release();

			renderer.resize(backBuffer, window.clientSize);

			depthView = renderer.createDepthStencilView(window.clientSize * superSampleWidth);

			projection = M4::projection((f32)window.clientSize.x / window.clientSize.y, DEG2RAD(75), 0.01f, farClipPlane);
			screenCBuffer.projection = projection;

			for (auto& rt : renderTargets)
				rt = renderer.createRenderTarget(window.clientSize * superSampleWidth);

			f32 ssOff = 0.0f;
			switch (superSampleWidth) {
				case 3: ssOff = 1.0f / 3.0f; break;
				case 4: ssOff = 0.25f; break;
			}
			auto invScreenSize = V2 {1} / V2 {window.clientSize};
			screenCBuffer.invScreenSize = invScreenSize / (f32)superSampleWidth;
			screenCBuffer.sampleOffset = invScreenSize * ssOff;
			screenCBuffer.update(renderer);
		}
		auto matrixCamPos = M4::translation(-cameraPos);
		auto matrixCamRot = M4::rotationYXZ(cameraRot);
		auto matrixCamRotProj = projection * matrixCamRot;
		auto matrixView = matrixCamRot * matrixCamPos;
		auto matrixVP = matrixCamRotProj * matrixCamPos;

		FrustumPlanes frustumPlanes { matrixVP };
		static std::vector<Chunk*> chunksToDraw;
		chunksToDraw.clear();

		world.chunkArena.for_each([&](Chunk* c) {
			if (frustumPlanes.containsSphere(V3 {(c->position - playerPos.chunkPos) * CHUNK_WIDTH + CHUNK_WIDTH / 2}, CHUNK_WIDTH / 2 * ROOT3)) {
				chunksToDraw.push_back(c);
			}
		});
		std::sort(chunksToDraw.begin(), chunksToDraw.end(), [&](Chunk* a, Chunk* b) {
			return V3 {a->position - playerPos.chunkPos}.lengthSqr() < V3 {b->position - playerPos.chunkPos}.lengthSqr();
		});

		renderer.clearRenderTargetView(backBuffer, V4 {1});
		renderer.clearRenderTargetView(renderTargets[0].rt, V4 {1});
		renderer.clearRenderTargetView(renderTargets[1].rt, V4 {1});
		renderer.clearDepthStencilView(depthView.view, D3D11_CLEAR_DEPTH, 1.0f, 0);

		renderer.setRenderTarget(renderTargets[0].rt, depthView.view);
		renderer.setViewport(window.clientSize* superSampleWidth);

		renderer.psSetShaderResource(2, atlasTex);
		renderer.psSetShaderResource(3, atlasNormalTex);
#if 0
		renderer.vsSetShader(approxVS);
		renderer.psSetShader(approxPS);
		for (auto& c : chunksToDraw) {
			c->drawApprox(drawCBuffer);
		}
		renderer.deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
#endif
		renderer.vsSetShader(chunkVS);
		renderer.psSetShader(chunkPS);

		drawCBuffer.camRotProjection = matrixCamRotProj;

		for (auto& c : chunksToDraw) {
			c->draw(drawCBuffer, playerPos.chunkPos, matrixVP, matrixCamPos);
		}

		{
			V3i placePos, breakPos;
			Hit hit;
			if (raycast(cameraPos, cameraPos + viewDir * 5, hit, placePos, isBreakable)) {
				breakPos = placePos;
				placePos += (V3i)hit.n;
				drawCBuffer.model = M4::translation((V3)breakPos) * M4::scaling(1.01f);
				drawCBuffer.modelCamPos = matrixCamPos * drawCBuffer.model;
				drawCBuffer.mvp = matrixVP * drawCBuffer.model;
				drawCBuffer.solidColor = {0,0,0,0.5};
				drawCBuffer.update(renderer);
				renderer.vsSetShader(blockVS);
				renderer.psSetShader(blockPS);
				renderer.psSetShaderResource(2, selectionTex);
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
		drawCBuffer.update(renderer);

		// SUN
		renderer.vsSetShader(sunVS);
		renderer.psSetShader(sunPS);
		renderer.draw(36);

		renderer.setRenderTarget(renderTargets[1].rt, 0);
		renderer.psSetShaderResource(0, renderTargets[0].sr);

		// SKY
		renderer.vsSetShader(skyVS);
		renderer.psSetShader(skyPS);
		renderer.draw(36);

		renderer.setViewport(window.clientSize);
		renderer.setRenderTarget(backBuffer, 0);

		// BLIT
		renderer.setViewport(window.clientSize);
		renderer.vsSetShader(blitVS);
		renderer.psSetShader(blitPS);
		renderer.psSetShaderResource(0, renderTargets[1].sr);
		//renderer.deviceContext->OMSetBlendState(alphaBlendInvertedOverwrite, blendFactor, 0xFFFFFFFF);
		renderer.draw(3);
		//renderer.deviceContext->OMSetBlendState(0, blendFactor, 0xFFFFFFFF);

		ID3D11ShaderResourceView* null = 0;
		renderer.psSetShaderResource(0, null);

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
		DHR(renderer.swapChain->Present(0, 0));
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
#else
		renderer.present(1);
#endif
		auto endCounter = WH::getPerformanceCounter();
		f32 frameTime = WH::getSecondsElapsed(lastCounter, endCounter, counterFrequency);
		lastCounter = endCounter;

#ifdef BUILD_RELEASE
		static u32 meshesBuiltInSec = 0;
		static f32 generateMS = 0;
		static f32 buildMeshMS = 0;
		static f32 secondTimer = 0;

		secondTimer += targetFrameTime;

		if (secondTimer >= 10) {
			secondTimer -= 10;
			generateMS = (f32)generateTime / generateCount / counterFrequency * 1000.f;
			buildMeshMS = (f32)buildMeshTime / buildMeshCount / counterFrequency * 1000.f;
			meshesBuiltInSec = meshesBuilt.exchange(0);
		}
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

		auto [ramValue, ramUnit] = normalizeBytes<double>(pmc.WorkingSetSize);
		auto [vramValue, vramUnit] = normalizeBytes<double>(renderer.getVRAM());
		sprintf(windowTitle, "gstorm - FPS: %.1f, Position: (%i/%i/%i) (%.2f/%.2f/%.2f), RAM usage: %.3f %s, VRAM usage: %.3f %s, Draws: %u/f, Gen. time: %.3fms/c, Mesh time: %.3fms/c, Meshes built: %u/s",
				1.f / frameTime,
				playerPos.chunkPos.x, playerPos.chunkPos.y, playerPos.chunkPos.z,
				playerPos.relPos.x, playerPos.relPos.y, playerPos.relPos.z,
				ramValue, ramUnit, vramValue, vramUnit, renderer.getDrawCalls(), generateMS, buildMeshMS, meshesBuiltInSec);
		SetWindowText(window.hwnd, windowTitle);
#else

		static u32 meshesBuiltInSec = 0;
		static f32 generateMS = 0;
		static f32 buildMeshMS = 0;
		static f32 secondTimer = 0;

		secondTimer += targetFrameTime;

		if (secondTimer >= 10) {
			secondTimer -= 10;
			generateMS = (f32)generateTime / generateCount / counterFrequency * 1000.f;
			buildMeshMS = (f32)buildMeshTime / buildMeshCount / counterFrequency * 1000.f;
			meshesBuiltInSec = meshesBuilt.exchange(0);
		}
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

		auto [ramValue, ramUnit] = normalizeBytes<double>(pmc.WorkingSetSize);
		auto [vramValue, vramUnit] = normalizeBytes<double>(renderer.getVRAM());
		sprintf(windowTitle, "gstorm - FPS: %.1f, Position: (%i/%i/%i) (%.2f/%.2f/%.2f), RAM usage: %.3f %s, VRAM usage: %.3f %s, Draws: %u/f, Gen. time: %.3fms/c, Mesh time: %.3fms/c, Loaded chunks: %zu, Meshes built: %u/s",
				1.f / frameTime,
				playerPos.chunkPos.x, playerPos.chunkPos.y, playerPos.chunkPos.z,
				playerPos.relPos.x, playerPos.relPos.y, playerPos.relPos.z,
				ramValue, ramUnit, vramValue, vramUnit, renderer.getDrawCalls(), generateMS, buildMeshMS, world.blockArena.occupiedCount, meshesBuiltInSec);
		SetWindowText(window.hwnd, windowTitle);

#endif
	}
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
