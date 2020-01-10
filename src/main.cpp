#pragma warning(disable:4189)//unused
#pragma warning(disable:4324)//align padding
#pragma warning(disable:4359)//align smaller
#pragma warning(disable:4201)//no name
#include <time.h>
#include <stdio.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <functional>
#include <optional>
#include "common.h"
#include "math.h"
#include "math.cpp"
#include "winhelper.h"
#include "d3d11helper.h"
#pragma comment(lib, "winmm")
#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_VISIBLE)
#define DATA "../data/"
#define WORLD_SURFACE 0
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
	V2i clientSize = {1280,720};
	bool running = true;
	bool resize = false;
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
		}
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
enum class BlockID : u8 {
	none, cube, sphere
};
struct Block {
	BlockID id {};
};
#define CHUNK_SIZE 32
#define TOTAL_CHUNK_SIZE (CHUNK_SIZE *CHUNK_SIZE * CHUNK_SIZE)
#define FOR_BLOCK_IN_CHUNK 		   \
for (int z=0; z < CHUNK_SIZE; ++z) \
for (int x=0; x < CHUNK_SIZE; ++x) \
for (int y=0; y < CHUNK_SIZE; ++y) 
#define BLOCK_INDEX(x,y,z) ((z) * CHUNK_SIZE * CHUNK_SIZE + (x) * CHUNK_SIZE + (y))
struct Vertex {
	V3 pos;
	V3 normal;
	V2 uv;
};
struct alignas(16) CBufferData {
	M4 mvp;
	V4 solidColor;
};
struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	ID3D11Buffer* vBuffer = 0;
	ID3D11Buffer* iBuffer = 0;
	ID3D11ShaderResourceView* vBufferView = 0;
	ID3D11ShaderResourceView* iBufferView = 0;
	u32 indexCount = 0;
	void load(ID3D11Device* device, char* const path, std::vector<Vertex>&& verticesi, std::vector<u32>&& indicesi) {
		vertices = std::move(verticesi);
		indices = std::move(indicesi);
#if 0
		u32 vertexSize = sizeof(Vertex);
		u32 indexSize = sizeof(u32);

		FILE* file = 0;
		//file = fopen(path, "rb");
		//if (!file) 
		{
			file = fopen(path, "wb");
			u32 version = 0;
			fwrite(&version, sizeof(version), 1, file);

			auto write = [&](auto val) {
				fwrite(&val, sizeof(val), 1, file);
			};

			write((u32)verticesi.size());
			fwrite(verticesi.data(), vertexSize * verticesi.size(), 1, file);
			write((u32)indicesi.size());
			fwrite(indicesi.data(), indexSize * indicesi.size(), 1, file);

			fclose(file);
			file = fopen(path, "rb");
		}
		u32 version;
		fread(&version, sizeof(version), 1, file);

		u32 vertexCount;
		fread(&vertexCount, sizeof(vertexCount), 1, file);

		u32 vertexBufferSize = vertexCount * vertexSize;

		std::vector<Vertex> vertices(vertexCount);
		fread(vertices.data(), vertexBufferSize, 1, file);

		createImmutableBuffer(device, D3D11_BIND_SHADER_RESOURCE, vertexBufferSize, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, vertexSize, &vBuffer, vertices.data());

		fread(&indexCount, sizeof(indexCount), 1, file);

		u32 indexBufferSize = indexCount * indexSize;

		std::vector<u32> indices(indexCount);
		fread(indices.data(), indexBufferSize, 1, file);

		createImmutableBuffer(device, D3D11_BIND_SHADER_RESOURCE, indexBufferSize, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, indexSize, &iBuffer, indices.data());

		fclose(file);

		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = vertexCount;
		DHR(device->CreateShaderResourceView(vBuffer, &desc, &vBufferView));
		desc.Buffer.NumElements = indexCount;
		DHR(device->CreateShaderResourceView(iBuffer, &desc, &iBufferView));

		assert(indexCount);
#else 
		u32 vertexSize = sizeof(Vertex);
		u32 indexSize = sizeof(u32);
		indexCount = (u32)indices.size();
		assert(indexCount);
		u32 vertexCount = (u32)vertices.size();
		u32 indexBufferSize = indexCount * indexSize;
		u32 vertexBufferSize = vertexCount * vertexSize;

		createConstStructuredBuffer(device, vertexBufferSize, vertexSize, &vBuffer, vertices.data());
		createConstStructuredBuffer(device, indexBufferSize,  indexSize,  &iBuffer, indices.data());

		createBufferView(device, vBuffer, vertexCount, &vBufferView);
		createBufferView(device, iBuffer, indexCount, &iBufferView);
#endif
	}
	void draw(ID3D11DeviceContext* deviceContext) {
		if (!indexCount)
			return;
		deviceContext->VSSetShaderResources(0, 1, &vBufferView);
		deviceContext->VSSetShaderResources(1, 1, &iBufferView);
		deviceContext->Draw(indexCount, 0);
	}
};
std::unordered_map<BlockID, Mesh> blockMeshes;
#define SIZEOF_CHUNK (sizeof(Block) * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)
#define SAVE_FILE DATA "save/world"
std::recursive_mutex saveFileMutex;
void printChunk(Block*, V3i);
struct Chunk {
	ID3D11Buffer* vBuffer = 0;
	ID3D11Buffer* iBuffer = 0;
	ID3D11ShaderResourceView* vBufferView = 0;
	ID3D11ShaderResourceView* iBufferView = 0;
	u32 indexCount = 0;
	bool needMeshRegen = false;
	bool generated = false;
	Block* blocks = 0;
	V3i position;
	Chunk(V3i position) : position(position) {

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
		if (iBuffer) iBuffer->Release();
		if (vBufferView) vBufferView->Release();
		if (iBufferView) iBufferView->Release();
	}
#if 1
	using FilePos = u32;
#define FILEPOS_NOT_LOADED ((FilePos)-1)
#define FILEPOS_APPEND ((FilePos)-2)
	FilePos filePos = FILEPOS_NOT_LOADED;
	auto hashChar() {
		return tohex(std::hash<V3i>()(position) & 0xF);
	}
	bool load(FILE* chunkFile) {
		assert(chunkFile);
		char hashFilePath[] = DATA "save/hashX";
		hashFilePath[sizeof(hashFilePath) - 2] = hashChar();
		auto hashFile = fopen(hashFilePath, "rb");
		if (!hashFile)
			return false;
		DEFER { fclose(hashFile); };

		fseek(hashFile, 0, SEEK_END);
		auto fileSize = ftell(hashFile);
		fseek(hashFile, 0, SEEK_SET);

		auto entrySize = sizeof(V3i) + sizeof(FilePos);
		assert(fileSize % entrySize == 0);

		auto entryCount = fileSize / entrySize;
		V3i entryPos;
		std::lock_guard g(saveFileMutex);
		for (int i=0; i < entryCount; ++i) {
			fread(&entryPos, sizeof(entryPos), 1, hashFile);
			if (entryPos == position) {
				fread(&filePos, sizeof(FilePos), 1, hashFile);
				assert(filePos % (sizeof(Block) * TOTAL_CHUNK_SIZE + sizeof(V3)) == 0);
				fseek(chunkFile, filePos, SEEK_SET);
				allocateBlocks();
				fseek(chunkFile, sizeof(entryPos), SEEK_CUR);
				fread(blocks, SIZEOF_CHUNK, 1, chunkFile);
				//printf("Loaded from %i ", ftell(chunkFile));
				//printChunk(blocks, position);
				generated = true;
				needMeshRegen = true;
				return true;
			}
			fseek(hashFile, sizeof(FilePos), SEEK_CUR);
		}
		return false;
	}
	void save(FILE* chunkFile) {
		assert(chunkFile);
		assert(filePos != FILEPOS_NOT_LOADED);
		
		assert(blocks);
		std::lock_guard g(saveFileMutex);
		if (filePos == FILEPOS_APPEND) {
			fseek(chunkFile, 0, SEEK_END);
			filePos = ftell(chunkFile);
			assert(filePos % (sizeof(Block) * TOTAL_CHUNK_SIZE + sizeof(V3i)) == 0);

			fwrite(&position, sizeof(position), 1, chunkFile);
			fwrite(blocks, SIZEOF_CHUNK, 1, chunkFile);

			char hashFilePath[] = DATA "save/hashX";
			hashFilePath[sizeof(hashFilePath) - 2] = hashChar();
			auto hashFile = fopen(hashFilePath, "ab");
			assert(hashFile);
			DEFER {fclose(hashFile);};

			fwrite(&position, sizeof(position), 1, hashFile);
			fwrite(&filePos, sizeof(filePos), 1, hashFile);
		}
		else {
			assert(filePos % (sizeof(Block) * TOTAL_CHUNK_SIZE + sizeof(V3i)) == 0);
			fseek(chunkFile, filePos, SEEK_SET);
			fseek(chunkFile, sizeof(V3i), SEEK_CUR);
			fwrite(blocks, SIZEOF_CHUNK, 1, chunkFile);
		}
		//printf("Saved to %i ", ftell(chunkFile));
		//printChunk(blocks, position);
	}
#else
	bool load(FILE* file) {
		TIMED_FUNCTION;
		fseek(file, 0, SEEK_END);
		auto fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);

		auto entrySize = sizeof(V3i) + SIZEOF_CHUNK;
		assert(fileSize % entrySize == 0);

		auto entryCount = fileSize / entrySize;
		V3i entryPos;
		for (int i=0; i < entryCount; ++i) {
			fread(&entryPos, sizeof(entryPos), 1, file);
			if (entryPos == position) {
				allocateBlocks();
				fread(blocks, SIZEOF_CHUNK, 1, file);
				generated = true;
				needUpdate = true;
				return true;
			}
			fseek(file, SIZEOF_CHUNK, SEEK_CUR);
		}
		return false;
	}
	void save(FILE* file) {
		TIMED_FUNCTION;
		assert(file);

		fseek(file, 0, SEEK_END);
		auto fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);

		auto entrySize = sizeof(V3i) + SIZEOF_CHUNK;
		assert(fileSize % entrySize == 0);

		auto entryCount = fileSize / entrySize;
		V3i entryPos;
		assert(blocks);
		for (int i=0; i < entryCount; ++i) {
			fread(&entryPos, sizeof(entryPos), 1, file);
			fseek(file, 0, SEEK_CUR);
			if (entryPos == position) {
				fwrite(blocks, SIZEOF_CHUNK, 1, file);
				return;
			}
			fseek(file, SIZEOF_CHUNK, SEEK_CUR);
		}
		fwrite(&position, sizeof(position), 1, file);
		fwrite(blocks, SIZEOF_CHUNK, 1, file);
	}
#endif
	bool setBlock(int x, int y, int z, Block blk) {
		assert(x >= 0 && x < CHUNK_SIZE);
		assert(y >= 0 && y < CHUNK_SIZE);
		assert(z >= 0 && z < CHUNK_SIZE);
		auto& b = blocks[BLOCK_INDEX(x,y,z)];
		b = blk;
		needMeshRegen = true;
		return true;
	}
	bool setBlock(V3i p, Block blk) {
		return setBlock(p.x, p.y, p.z, blk);
	}
	Block getBlock(int x, int y, int z) {
		assert(x >= 0 && x < CHUNK_SIZE);
		assert(y >= 0 && y < CHUNK_SIZE);
		assert(z >= 0 && z < CHUNK_SIZE);
		return blocks[BLOCK_INDEX(x, y, z)];
	}
	Block getBlock(V3i p) {
		return getBlock(p.x, p.y, p.z);
	}
	void generate(ID3D11Device* device) {
		TIMED_FUNCTION;
		filePos = FILEPOS_APPEND;
		allocateBlocks();
		if (generated)
			return;
		//printf("Generated ");
		//printChunk(blocks, position);
#if WORLD_SURFACE
		if (position.y != 0)
			return;
		for (int x = 0; x < CHUNK_SIZE; ++x) {
			for (int z = 0; z < CHUNK_SIZE; ++z) {
				V3i abs { position.x * CHUNK_SIZE + x, 0, position.z * CHUNK_SIZE + z };
				int h = (int)(1 + perlin(abs.x / 32.0f / PI + 100000, abs.z / 32.0f / PI + 100000, 8) * 30);
				for (int y = 0; y < h; ++y) {
					blocks[BLOCK_INDEX(x, y, z)].id = BlockID::cube;
				}
			}
		}
#else
		FOR_BLOCK_IN_CHUNK {
			if (voronoi((V3(position * CHUNK_SIZE) + V3((f32)x, (f32)y, (f32)z)) / 64.0f * PI) < 0.1f)
				blocks[BLOCK_INDEX(x, y, z)].id = BlockID::cube;
		}
#endif
		needMeshRegen = true;
		generateMesh(device);
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		generated = true;
	}
	void generateMesh(ID3D11Device* device) {
		TIMED_FUNCTION;
		if (!needMeshRegen)
			return;
		needMeshRegen = false;
#define RESERVE_COUNT 2
		std::vector<Vertex> vertices;
		vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 4 * RESERVE_COUNT);
		std::vector<u32> indices;
		indices.reserve(CHUNK_SIZE * CHUNK_SIZE * 6 * RESERVE_COUNT);
#undef RESERVE_COUNT
		u32 idxOff = 0;
		FOR_BLOCK_IN_CHUNK {
			auto& b = blocks[BLOCK_INDEX(x,y,z)];
			switch (b.id) {
				case BlockID::none: continue;
				//case BlockID::cube: 
				case BlockID::sphere: {
					auto& mesh = blockMeshes.at(b.id);
					for (auto v : mesh.vertices) {
						v.pos += V3 {(f32)x,(f32)y,(f32)z};
						vertices.push_back(v);
					}
					for (u32 i : mesh.indices) {
						indices.push_back(i + idxOff);
					}
					idxOff += (u32)mesh.vertices.size();
					break; 
				}
#if 1
				case BlockID::cube: {
					bool visible[6];
					visible[0] = x == CHUNK_SIZE - 1 ? true : blocks[BLOCK_INDEX(x + 1, y, z)].id != BlockID::cube;
					visible[1] = x == 0              ? true : blocks[BLOCK_INDEX(x - 1, y, z)].id != BlockID::cube;
					visible[2] = y == CHUNK_SIZE - 1 ? true : blocks[BLOCK_INDEX(x, y + 1, z)].id != BlockID::cube;
					visible[3] = y == 0              ? true : blocks[BLOCK_INDEX(x, y - 1, z)].id != BlockID::cube;
					visible[4] = z == CHUNK_SIZE - 1 ? true : blocks[BLOCK_INDEX(x, y, z + 1)].id != BlockID::cube;
					visible[5] = z == 0              ? true : blocks[BLOCK_INDEX(x, y, z - 1)].id != BlockID::cube;
					auto oldSize = vertices.size();
					auto insertIndices = [&]() {
						indices.insert(indices.end(), {
							0 + idxOff,
							1 + idxOff,
							2 + idxOff,
							1 + idxOff,
							3 + idxOff,
							2 + idxOff,
						});
						idxOff += 4;
					};
					if (visible[0]) {
						vertices.insert(vertices.end(), {
							{{ 0.5, 0.5,-0.5}, {1,0,0}, {0,1}}, {{ 0.5, 0.5, 0.5}, {1,0,0}, {1,1}},// +x
							{{ 0.5,-0.5,-0.5}, {1,0,0}, {0,0}}, {{ 0.5,-0.5, 0.5}, {1,0,0}, {1,0}},
						});
						insertIndices();
					}
					if (visible[1]) {
						vertices.insert(vertices.end(), {
							{{-0.5, 0.5, 0.5}, {-1,0,0}, {0,1}}, {{-0.5, 0.5,-0.5}, {-1,0,0}, {1,1}},// -x
							{{-0.5,-0.5, 0.5}, {-1,0,0}, {0,0}}, {{-0.5,-0.5,-0.5}, {-1,0,0}, {1,0}},
						});
						insertIndices();
					}
					if (visible[2]) {
						vertices.insert(vertices.end(), {
							{{-0.5, 0.5, 0.5}, {0,1,0}, {0,1}}, {{ 0.5, 0.5, 0.5}, {0,1,0}, {1,1}},// +y
							{{-0.5, 0.5,-0.5}, {0,1,0}, {0,0}}, {{ 0.5, 0.5,-0.5}, {0,1,0}, {1,0}},
										});
						insertIndices();
					}
					if (visible[3]) {
						vertices.insert(vertices.end(), {
							{{-0.5,-0.5,-0.5}, {0,-1,0}, {0,1}}, {{ 0.5,-0.5,-0.5}, {0,-1,0}, {1,1}},// -y
							{{-0.5,-0.5, 0.5}, {0,-1,0}, {0,0}}, {{ 0.5,-0.5, 0.5}, {0,-1,0}, {1,0}},
										});
						insertIndices();
					}
					if (visible[4]) {
						vertices.insert(vertices.end(), {
							{{ 0.5, 0.5, 0.5}, {0,0,1}, {0,1}}, {{-0.5, 0.5, 0.5}, {0,0,1}, {1,1}},// +z
							{{ 0.5,-0.5, 0.5}, {0,0,1}, {0,0}}, {{-0.5,-0.5, 0.5}, {0,0,1}, {1,0}},
										});
						insertIndices();
					}
					if (visible[5]) {
						vertices.insert(vertices.end(), {
							{{-0.5, 0.5,-0.5}, {0,0,-1}, {0,1}}, {{ 0.5, 0.5,-0.5}, {0,0,-1}, {1,1}},// -z
							{{-0.5,-0.5,-0.5}, {0,0,-1}, {0,0}}, {{ 0.5,-0.5,-0.5}, {0,0,-1}, {1,0}},
										});
						insertIndices();
					}
					for (auto i=oldSize; i < vertices.size(); ++i) {
						vertices[i].pos += V3 {(f32)x,(f32)y,(f32)z};
					}
					break;
				}
#endif
				default:
					break;
			}
		}

		u32 vertexSize = sizeof(Vertex);
		u32 indexSize = sizeof(u32);
		indexCount = (u32)indices.size();
		u32 vertexCount = (u32)vertices.size();
		u32 indexBufferSize = indexCount * indexSize;
		u32 vertexBufferSize = vertexCount * vertexSize;

		if (vBuffer) vBuffer->Release();
		if (iBuffer) iBuffer->Release();
		if (vBufferView) vBufferView->Release();
		if (iBufferView) iBufferView->Release();

		if (!indexCount)
			return;

		createConstStructuredBuffer(device, vertexBufferSize, vertexSize, &vBuffer, vertices.data());
		createConstStructuredBuffer(device, indexBufferSize, indexSize, &iBuffer, indices.data());

		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = vertexCount;
		DHR(device->CreateShaderResourceView(vBuffer, &desc, &vBufferView));
		desc.Buffer.NumElements = indexCount;
		DHR(device->CreateShaderResourceView(iBuffer, &desc, &iBufferView));
	}
	void draw(ID3D11DeviceContext* deviceContext, V3i playerChunk, M4& projection, V3& cameraRot, V3& cameraPos, ID3D11Buffer* cBuffer) {
		if (!indexCount || !vBufferView || !iBufferView)
			return;

		CBufferData data;
		data.solidColor = 1;
		data.mvp = projection * M4::rotationYXZ(cameraRot) * M4::translation(-cameraPos) * M4::translation((V3)((position - playerChunk) * CHUNK_SIZE));
		updateBuffer(deviceContext, cBuffer, data);
		deviceContext->VSSetShaderResources(0, 1, &vBufferView);
		deviceContext->VSSetShaderResources(1, 1, &iBufferView);
		deviceContext->Draw(indexCount, 0);
	}
};
void printChunk(Block* blocks, V3i pos) {
	auto usedBlocks = 0;
	for (int j = 0; j < TOTAL_CHUNK_SIZE; ++j) {
		if (blocks[j].id == BlockID::cube) ++usedBlocks;
	}
	printf("chunk %i %i %i, used blocks: %u\n", pos.x, pos.y, pos.z, usedBlocks);
}
V3i getBlockRelPos(V3i worldPos) {
	worldPos.x %= CHUNK_SIZE;
	worldPos.y %= CHUNK_SIZE;
	worldPos.z %= CHUNK_SIZE;
	if (worldPos.x < 0) worldPos.x = CHUNK_SIZE + worldPos.x;
	if (worldPos.y < 0) worldPos.y = CHUNK_SIZE + worldPos.y;
	if (worldPos.z < 0) worldPos.z = CHUNK_SIZE + worldPos.z;
	return worldPos;
}
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
struct ChunkArena {
	std::vector<char> memory;
	void allocate() {

	}
};
struct World {
	std::unordered_map<V3i, Chunk*> chunks;
	struct Queue {
		std::vector<Chunk*> buffers[2];
		std::vector<Chunk*>* current = buffers, *next = buffers + 1;
		std::recursive_mutex currentMutex, nextMutex;
		void push(Chunk* chunk) {
			nextMutex.lock();
			next->push_back(chunk);
			nextMutex.unlock();
		}
		void swap() {
			nextMutex.lock();
			std::swap(current, next);
			nextMutex.unlock();
		}
	} loadQueue, unloadQueue;
	std::unordered_set<Chunk*> visibleChunks;
	World() {
		auto file = fopen(SAVE_FILE, "rb");
		if (!file)
			return;

		fseek(file, 0, SEEK_END);
		auto fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);

		auto entrySize = SIZEOF_CHUNK + sizeof(V3i);
		assert(fileSize % entrySize == 0);

		auto entryCount = fileSize / entrySize;
		V3i entryPos;
		Block blocks[TOTAL_CHUNK_SIZE];
		for (int i = 0; i < entryCount; ++i) {
			fread(&entryPos, sizeof(entryPos), 1, file);
			fread(blocks, SIZEOF_CHUNK, 1, file);
			//printChunk(blocks, entryPos);
		}
		fclose(file);
	}
	void seeChunk(V3i pos) {
		visibleChunks.emplace(getChunkUnchecked(pos));
	}
	// main thread
	void unseeChunks(V3i playerChunk, int drawDistance, int unloadDistance) {
		std::vector<Chunk*> toUnsee;
		toUnsee.reserve(drawDistance * 2 + 1);
		for (auto c : visibleChunks) {
			auto dist = (playerChunk - c->position).absolute();
			if (dist.x > drawDistance || dist.y > drawDistance || dist.z > drawDistance) {
				toUnsee.push_back(c);
			}
		}
		for (auto c : toUnsee)
			visibleChunks.erase(c);

		std::vector<Chunk*> chunksToUnload;
		chunksToUnload.reserve(unloadDistance * 2 + 1);
		for (auto c : chunks) {
			auto dist = (playerChunk - c.second->position).absolute();
			if (dist.x > unloadDistance || dist.y > unloadDistance || dist.z > unloadDistance) {
				unloadQueue.push(c.second);
				chunksToUnload.push_back(c.second);
			}
		}
		for (auto c : chunksToUnload) {
			chunks.erase(c->position);
		}
	}
	void saveFarChunks() {
	}
	// main thread
	void save() {
		TIMED_FUNCTION;
		auto file = openFileRW(SAVE_FILE);
		for (auto c : chunks) {
			c.second->save(file);
			c.second->free();
			delete c.second;
		}
		fclose(file);
	}
	// chunk loader thread
	void updateChunks(ID3D11Device* device) {
		/* LOAD CHUNKS */ {
			loadQueue.currentMutex.lock();
			DEFER {
				loadQueue.swap();
				loadQueue.currentMutex.unlock();
			};
			if (loadQueue.current->empty()) 
				return;
			printf("To load: %zu\n", loadQueue.current->size());
			{
				auto file = fopen(SAVE_FILE, "rb");
				if (file) {
					for (auto c : *loadQueue.current) {
						if (!c->load(file))
							c->generate(device);
					}
					fclose(file);
				}
				else {
					for (auto c : *loadQueue.current) {
						c->generate(device);
					}
				}
			}
			loadQueue.current->clear();
		}
		/* UNLOAD CHUNKS */ {
			unloadQueue.currentMutex.lock();
			DEFER {
				unloadQueue.swap();
				unloadQueue.currentMutex.unlock();
			};
			if (unloadQueue.current->empty()) 
				return;
			printf("To unload: %zu\n", unloadQueue.current->size());
			auto file = openFileRW(SAVE_FILE);
			for (auto c : *unloadQueue.current) {
				saveFileMutex.lock();
				c->save(file);
				saveFileMutex.unlock();
				c->free();
				delete c;
			}
			unloadQueue.current->clear();
			fclose(file);
		}
	}
	Chunk* getChunkUnchecked(V3i pos) {
		if (auto it = chunks.find(pos); it != chunks.end())
			return it->second;
		auto chunk = new Chunk(pos);
		chunks[pos] = chunk;
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
	bool setBlock(V3i worldPos, Block blk) {
		auto c = getChunkFromBlock(worldPos);
		if (!c)
			return false;
		return c->setBlock(getBlockRelPos(worldPos), blk);
	}
	bool setBlock(V3 pos, Block blk) {
		return setBlock((V3i)pos, blk);
	}
	bool destroyBlock(V3i pos) {
		auto c = getChunkFromBlock(pos);
		if (!c)
			return false;
		return c->setBlock(getBlockRelPos(pos), {BlockID::none});
	}
	bool hasBlock(V3i pos) {
		auto c = getChunkFromBlock(pos);
		if (!c)
			return false;
		return c->getBlock(getBlockRelPos(pos)).id != BlockID::none;
	}
};
struct Position {
	void setWorld(V3 pos) {
		relPos = pos;
		chunkPos = {};
		normalize();
	}
	V3i getChunk() {
		return chunkPos;
	}
	V3 getWorld() {
		return V3 {chunkPos * CHUNK_SIZE} +relPos;
	}
	V3 getRelative() {
		return relPos;
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
private:
	V3 relPos;
	V3i chunkPos;
};
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);


	const f32 maxSpeed = 15;
	const f32 jumpForce = 250;
	const f32 airMult = 0.2;
	const f32 noclipMult = 16;
	const f32 groundFriction = 1;
	const f32 airFriction = 0.2f;
	const f32 noclipFriction = 1;
	const f32 noclipMaxSpeed = 50;
	const f32 camHeight = 0.625f;

	Position playerPos;

	V3 spawnPos {0,32,0};
	playerPos.setWorld(spawnPos);
	V3 playerVel;
	V3 playerDimH = {0.375f, 0.875f, 0.375f};
	V3 cameraRot;

	puts("Введи дистанцию прорисовки (рекомендую от 2 до 8)");
	int chunkDrawDistance = 16;
	std::cin >> chunkDrawDistance;
	if (chunkDrawDistance <= 0 || chunkDrawDistance > 32) {
		chunkDrawDistance = 8;
		puts("так низя. ставлю 8");
	}
	int chunkUnloadDistance = chunkDrawDistance * 2;


	WndClassExA wndClass;
	wndClass.hInstance = instance;
	wndClass.lpfnWndProc = wndProc;
	wndClass.lpszClassName = "gstorm";
	wndClass.checkIn();

	auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
	auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

	Window window;

	Rect windowRect;
	windowRect.xywh((screenWidth - (LONG)window.clientSize.x) / 2,
		(screenHeight - (LONG)window.clientSize.y) / 2,
					(LONG)window.clientSize.x,
					(LONG)window.clientSize.y);

	AdjustWindowRect(&windowRect, WINDOW_STYLE, 0);

	window.hwnd = CreateWindowA(wndClass.lpszClassName, "Galaxy Storm", WINDOW_STYLE,
								windowRect.left, windowRect.top, windowRect.width(), windowRect.height(), 0, 0, instance, &window);
	assert(window.hwnd);

	RAWINPUTDEVICE RawInputMouseDevice = {};
	RawInputMouseDevice.usUsagePage = 0x01;
	RawInputMouseDevice.usUsage = 0x02;

	if (!RegisterRawInputDevices(&RawInputMouseDevice, 1, sizeof(RAWINPUTDEVICE))) {
		assert(0);
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc {};
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Width = window.clientSize.x;
	swapChainDesc.BufferDesc.Height = window.clientSize.y;
	swapChainDesc.BufferDesc.RefreshRate = {60, 1};
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = window.hwnd;
	swapChainDesc.SampleDesc = {1, 0};
	swapChainDesc.Windowed = 1;

	IDXGISwapChain* swapChain = 0;
	ID3D11Device* device = 0;
	ID3D11DeviceContext* deviceContext = 0;

	DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, CREATE_DEVICE_FLAGS,
									  0, 0, D3D11_SDK_VERSION, &swapChainDesc,
									  &swapChain, &device, 0, &deviceContext));

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11RenderTargetView* backBuffer = 0;
	ID3D11DepthStencilView* depthView = 0;
	ID3D11Texture2D* depthTex = 0;

	ID3D11VertexShader* vertexShader = 0;
	ID3D11PixelShader* pixelShader = 0;
	createVertexShader(device, DATA "shaders/basic.hlsl", &vertexShader);
	createPixelShader(device, DATA "shaders/basic.hlsl", &pixelShader);
	deviceContext->VSSetShader(vertexShader, 0, 0);
	deviceContext->PSSetShader(pixelShader, 0, 0);

	CBufferData cBufferData {};
	M4 projection;
	ID3D11Buffer* cBuffer = 0;
	createDynamicBuffer(device, D3D11_BIND_CONSTANT_BUFFER, sizeof(cBufferData), 0, 0, &cBuffer);
	deviceContext->VSSetConstantBuffers(0, 1, &cBuffer);

	blockMeshes[BlockID::cube].load(device, DATA "mesh/block.mesh", {
					  {{-0.5, 0.5,-0.5}, {0,0,-1}, {0,1}}, {{ 0.5, 0.5,-0.5}, {0,0,-1}, {1,1}},// -z
					  {{-0.5,-0.5,-0.5}, {0,0,-1}, {0,0}}, {{ 0.5,-0.5,-0.5}, {0,0,-1}, {1,0}},
					  {{ 0.5, 0.5, 0.5}, {0,0,1}, {0,1}}, {{-0.5, 0.5, 0.5}, {0,0,1}, {1,1}},// +z
					  {{ 0.5,-0.5, 0.5}, {0,0,1}, {0,0}}, {{-0.5,-0.5, 0.5}, {0,0,1}, {1,0}},
					  {{-0.5,-0.5,-0.5}, {0,-1,0}, {0,1}}, {{ 0.5,-0.5,-0.5}, {0,-1,0}, {1,1}},// -y
					  {{-0.5,-0.5, 0.5}, {0,-1,0}, {0,0}}, {{ 0.5,-0.5, 0.5}, {0,-1,0}, {1,0}},
					  {{-0.5, 0.5, 0.5}, {0,1,0}, {0,1}}, {{ 0.5, 0.5, 0.5}, {0,1,0}, {1,1}},// +y
					  {{-0.5, 0.5,-0.5}, {0,1,0}, {0,0}}, {{ 0.5, 0.5,-0.5}, {0,1,0}, {1,0}},
					  {{-0.5, 0.5, 0.5}, {-1,0,0}, {0,1}}, {{-0.5, 0.5,-0.5}, {-1,0,0}, {1,1}},// -x
					  {{-0.5,-0.5, 0.5}, {-1,0,0}, {0,0}}, {{-0.5,-0.5,-0.5}, {-1,0,0}, {1,0}},
					  {{ 0.5, 0.5,-0.5}, {1,0,0}, {0,1}}, {{ 0.5, 0.5, 0.5}, {1,0,0}, {1,1}},// +x
					  {{ 0.5,-0.5,-0.5}, {1,0,0}, {0,0}}, {{ 0.5,-0.5, 0.5}, {1,0,0}, {1,0}},
					}, {
						0,1,2,1,3,2,
						4,5,6,5,7,6,
						8,9,10,9,11,10,
						12,13,14,13,15,14,
						16,17,18,17,19,18,
						20,21,22,21,23,22,
				   });
	blockMeshes[BlockID::sphere].load(device, DATA "mesh/sphere.mesh", {
						{{ 0.5f, 0.0f, 0.0f}, {1,0,0}},
						{{-0.5f, 0.0f, 0.0f}, {-1,0,0}},
						{{ 0.0f, 0.5f, 0.0f}, {0,1,0}},
						{{ 0.0f,-0.5f, 0.0f}, {0,-1,0}},
						{{ 0.0f, 0.0f, 0.5f}, {0,0,1}},
						{{ 0.0f, 0.0f,-0.5f}, {0,0,-1}},
					}, {
						0,2,4,0,5,2,0,3,5,0,4,3,
						1,2,5,1,5,3,1,3,4,1,4,2
					});

	ID3D11ShaderResourceView* grassTex = 0;
	DHR(D3DX11CreateShaderResourceViewFromFileA(device, DATA "textures/dirt.png", 0, 0, &grassTex, 0));
	deviceContext->PSSetShaderResources(2, 1, &grassTex);

	ID3D11SamplerState* samplerState = 0;
	{
		D3D11_SAMPLER_DESC desc {};
		desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		desc.MaxAnisotropy = 16;
		desc.MaxLOD = FLT_MAX;
		DHR(device->CreateSamplerState(&desc, &samplerState));
	}

	deviceContext->PSSetSamplers(0, 1, &samplerState);

	ID3D11BlendState* outlineBlend = 0;
	{
		D3D11_BLEND_DESC desc {};
		desc.RenderTarget[0].BlendEnable = 1;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		DHR(device->CreateBlendState(&desc, &outlineBlend));
	}

	f32 blendFactor[4] {1,1,1,1};

#define PATH_SAVE DATA "world.save"

	World world;

	std::thread chunkLoader([device, &window, &world]() {
		while (window.running) {
			world.updateChunks(device);
		}
	});

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
		auto playerChunk = playerPos.getChunk();
		world.loadQueue.nextMutex.lock();
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
					for (int y = -r; y <= r; ++y) {
						for (int z = -r; z <= r; ++z) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (x == r)
						break;
					x = r;
				}
			}
			{
				int y = -r;
				while (1) {
					for (int x = -r + 1; x <= r - 1; ++x) {
						for (int z = -r; z <= r; ++z) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (y == r)
						break;
					y = r;
				}
			}
			{
				int z = -r;
				while (1) {
					for (int x = -r + 1; x <= r - 1; ++x) {
						for (int y = -r + 1; y <= r - 1; ++y) {
							world.seeChunk(playerChunk + V3i {x,y,z});
						}
					}
					if (z == r)
						break;
					z = r;
				}
			}
		}
#endif
		world.loadQueue.nextMutex.unlock();
		world.unseeChunks(playerPos.getChunk(), chunkDrawDistance, chunkUnloadDistance);
	};
	loadWorld();

	enum class MoveMode {
		walk,
		fly,
		noclip,
		count
	};

	MoveMode moveMode {};

	BlockID toolBlock = BlockID::cube;

	MSG msg {};

	float targetFrameTime = 1.0f / 60.0f;

	LARGE_INTEGER performaceFrequency;
	QueryPerformanceFrequency(&performaceFrequency);
	auto counterFrequency = performaceFrequency.QuadPart;

	bool sleepIsAccurate = timeBeginPeriod(1) == TIMERR_NOERROR;
	if (sleepIsAccurate)
		puts("Sleep() is accurate");
	else
		puts("Sleep() is inaccurate!!!");

#define GRAVITY 13
	bool grounded = false;

	Input input;

	auto lastCounter = getPerformanceCounter();
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
		}

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
		else if (moveMode == MoveMode::noclip) {
			accelMult *= noclipMult;
		}
		else if (moveMode == MoveMode::walk) {
			accelMult *= airMult;
		}

		if (moveMode != MoveMode::walk)
			accelMult *= (noclipMaxSpeed - playerVel.length()) / noclipMaxSpeed;
		else
			accelMult *= (maxSpeed - playerVel.xz().length()) / maxSpeed;

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

		auto oldPlayerPos = playerPos.getWorld();
		auto newPlayerPos = oldPlayerPos;
		if (moveMode == MoveMode::noclip) {
			grounded = false;
		} 
		else {
			// raycast
			bool newGrounded = false;
			for (int iter = 0; iter < 4; ++iter) {
				V3 begin = newPlayerPos;
				V3 end = begin + playerVel * targetFrameTime;
				f32 targetTravelLenSqr = (begin - end).lengthSqr();
				if (targetTravelLenSqr == 0)
					break;
				V3 min, max;
				auto minmax = [](auto a, auto b, auto& omin, auto& omax) {
					if (a < b) {
						omin = a;
						omax = b;
					}
					else {
						omin = b;
						omax = a;
					}
				};
				struct Hit {
					V3 p, n;
				};
				auto raycastPlane = [](V3 a, V3 b, V3 s1, V3 s2, V3 s3, Hit& hit) {
					// 1.
					auto dS21 = s2 - s1;
					auto dS31 = s3 - s1;
					hit.n = dS21.cross(dS31).normalized();

					// 2.
					auto dR = a - b;

					f32 ndotdR = hit.n.dot(dR);

					if (ndotdR <= 1e-5f) { // Choose your tolerance
						return false;
					}

					f32 t = -hit.n.dot(a - s1) / ndotdR;
					hit.p = a + dR * t;

					// 3.
					auto dMS1 = hit.p - s1;
					float u = dMS1.dot(dS21);
					float v = dMS1.dot(dS31);

					if ((a - hit.p).lengthSqr() > (a - b).lengthSqr() || (a - hit.p).dot(a - b) <= 0) {
						return false;
					}

					// 4.
					return (u >= 0.0f && u <= dS21.dot(dS21)
							&& v >= 0.0f && v <= dS31.dot(dS31));
				};
				auto raycastBlock = [raycastPlane, playerDimH](V3 a, V3 b, V3 blk, Hit& hit) {
					Hit hits[6];
					bool results[6] {};
					f32 x = 0.5f + playerDimH.x;
					f32 y = 0.5f + playerDimH.y;
					f32 z = 0.5f + playerDimH.z;
					results[0] = raycastPlane(a, b, blk + V3 {x, y, z}, blk + V3 {x,-y, z}, blk + V3 {x, y,-z}, hits[0]);//+x
					results[1] = raycastPlane(a, b, blk + V3 {-x, y, z}, blk + V3 {-x, y,-z}, blk + V3 {-x,-y, z}, hits[1]);//-x
					results[2] = raycastPlane(a, b, blk + V3 {x, y, z}, blk + V3 {x, y,-z}, blk + V3 {-x, y, z}, hits[2]);//+y
					results[3] = raycastPlane(a, b, blk + V3 {x,-y, z}, blk + V3 {-x,-y, z}, blk + V3 {x,-y,-z}, hits[3]);//-y
					results[4] = raycastPlane(a, b, blk + V3 {x, y, z}, blk + V3 {-x, y, z}, blk + V3 {x,-y, z}, hits[4]);//+z
					results[5] = raycastPlane(a, b, blk + V3 {x, y,-z}, blk + V3 {x,-y,-z}, blk + V3 {-x, y,-z}, hits[5]);//-z
					int min = -1;
					f32 minDist = FLT_MAX;
					for (int i = 0; i < 6; ++i) {
						if (results[i]) {
							auto lenSqr = (a - hits[i].p).lengthSqr();
							if (lenSqr < minDist) {
								minDist = lenSqr;
								min = i;
							}
						}
					}
					if (min == -1)
						return false;
					//switch (min) {
					//	case 0: puts("+x"); break;
					//	case 1: puts("-x"); break;
					//	case 2: puts("+y"); break;
					//	case 3: puts("-y"); break;
					//	case 4: puts("+z"); break;
					//	case 5: puts("-z"); break;
					//	default:
					//		break;
					//}
					hit = hits[min];
					return true;
				};
				auto isInsideBlock = [&](V3 pos) {
					V3i mini = (pos - playerDimH).rounded();
					V3i maxi = (pos + playerDimH).rounded();
					for (int z = mini.z; z <= maxi.z; ++z) {
						for (int y = mini.y; y <= maxi.y; ++y) {
							for (int x = mini.x; x <= maxi.x; ++x) {
								if (world.hasBlock({x,y,z})) {
									return true;
								}
							}
						}
					}
					return false;

				};
				minmax(begin.x, end.x, min.x, max.x);
				minmax(begin.y, end.y, min.y, max.y);
				minmax(begin.z, end.z, min.z, max.z);
				min -= playerDimH;
				max += playerDimH;
				V3i mini = min.rounded();
				V3i maxi = max.rounded();
				Hit hit;
				V3i hitBlock;
				f32 minDist = FLT_MAX;
				for (int z = mini.z; z <= maxi.z; ++z) {
					for (int y = mini.y; y <= maxi.y; ++y) {
						for (int x = mini.x; x <= maxi.x; ++x) {
							V3i blk = {x,y,z};
							if (world.hasBlock(blk)) {
								Hit hit_;
								if (raycastBlock(begin, end, (V3)blk, hit_)) {
									auto lenSqr = (begin - hit_.p).lengthSqr();
									if (lenSqr < minDist) {
										minDist = lenSqr;
										hit = hit_;
										hitBlock = blk;
									}
								}
							}
						}
					}
				}
				if (hit.n.lengthSqr() > 0) {
					// TODO FIX:
					if ((newPlayerPos - hit.p).lengthSqr() > targetTravelLenSqr) {
						break;
					}
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
						V3 hitBlockF = (V3)hitBlock;
						if (raycastPlane(a, b, hitBlockF + V3 {x, y, z}, hitBlockF + V3 {x, y,-z}, hitBlockF + V3 {-x, y, z}, stepHit)) {
							auto newPos = stepHit.p + V3 {0,1e-3f,0};
							if (!isInsideBlock(newPos)) {
								newPlayerPos = newPos;
								continue;
							}
						}
					}
					newPlayerPos = hit.p + hit.n * 1e-3f;
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
			newPlayerPos = command;
		}
		else {
			newPlayerPos += playerVel * targetFrameTime;
		}

#if WORLD_SURFACE
		if (!grounded && newPlayerPos.y < -10) {
			newPlayerPos = spawnPos;
			playerVel = 0;
		}
#endif

		auto chunkChanged = playerPos.move(newPlayerPos - oldPlayerPos);

		if (chunkChanged) {
			loadWorld();
		}

		auto cameraPos = playerPos.getRelative();
		cameraPos.y += camHeight;
		V3 view = M4::rotationZXY(-cameraRot) * V3 { 0, 0, 1 };

		if (window.resize) {
			window.resize = false;

			if (backBuffer) {
				backBuffer->Release();
				depthView->Release();
				depthTex->Release();
			}

			DHR(swapChain->ResizeBuffers(2, window.clientSize.x, window.clientSize.y, DXGI_FORMAT_UNKNOWN, 0));

			ID3D11Texture2D* backBufferTexture = 0;
			DHR(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture)));
			DHR(device->CreateRenderTargetView(backBufferTexture, 0, &backBuffer));
			backBufferTexture->Release();

			D3D11_TEXTURE2D_DESC desc {};
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			desc.Width = window.clientSize.x;
			desc.Height = window.clientSize.y;
			desc.MipLevels = 1;
			desc.SampleDesc = {1, 0};
			DHR(device->CreateTexture2D(&desc, 0, &depthTex));
			DHR(device->CreateDepthStencilView(depthTex, 0, &depthView));

			setViewport(deviceContext, window.clientSize);
			projection = M4::projection((f32)window.clientSize.x / window.clientSize.y, DEG2RAD(90), 0.01f, 1000.0f);

			deviceContext->OMSetRenderTargets(1, &backBuffer, depthView);
		}

		float color[4] {0.05f, 0.05f, 0.2f, 1.0f};
		deviceContext->ClearRenderTargetView(backBuffer, color);
		deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		auto drawChunk = [&](Chunk* c) {
			if (c->needMeshRegen)
				c->generateMesh(device);
			c->draw(deviceContext, playerPos.getChunk(), projection, cameraRot, cameraPos, cBuffer);
		};
		auto drawBlock = [&](BlockID id) {
			switch (id) {
				case BlockID::cube:   break;
				case BlockID::sphere: break;
				default:
					break;
			}
		};
		for (auto& c : world.visibleChunks) {
			drawChunk(c);
		}
		//for (auto& c : world.chunks) {
		//	drawChunk(c.second);
		//}
		V3 bpos = cameraPos + view * 3;
		bpos.x = roundf(bpos.x);
		bpos.y = roundf(bpos.y);
		bpos.z = roundf(bpos.z);
#if 1
		deviceContext->OMSetBlendState(outlineBlend, blendFactor, 0xFFFFFFFF);
		/*
		cBufferData.solidColor = {1,1,1,0.1};
		for (int z = -1; z < 1; ++z)
			for (int y = -1; y < 1; ++y)
				for (int x = -1; x < 1; ++x) {
					cBufferData.mvp = projection * M4::rotationYXZ(cameraRot) * M4::translation(-cameraPos) * M4::translation(V3 {(f32)x,(f32)y,(f32)z} * 16 + 7.5f) * M4::scaling(-16);
					updateBuffer(deviceContext, cBuffer, cBufferData);
					blockMeshes[BlockID::cube].draw(deviceContext);
				}
				*/
		cBufferData.solidColor = {2,2,2, 0.25};
		cBufferData.mvp = projection * M4::rotationYXZ(cameraRot) * M4::translation(-cameraPos) * M4::translation(bpos) * M4::scaling(-1.1f);
		updateBuffer(deviceContext, cBuffer, cBufferData);
		blockMeshes.at(toolBlock).draw(deviceContext);
		cBufferData.mvp = projection * M4::rotationYXZ(cameraRot) * M4::translation(-cameraPos) * M4::translation(bpos) * M4::scaling(-0.9f);
		updateBuffer(deviceContext, cBuffer, cBufferData);
		blockMeshes.at(toolBlock).draw(deviceContext);
		deviceContext->OMSetBlendState(0, blendFactor, 0xFFFFFFFF);
		cBufferData.solidColor = 1;
#endif
		if (input.keyDown('1')) {
			toolBlock = BlockID::cube;
			puts("Block: cube");
		}
		else if (input.keyDown('2')) {
			toolBlock = BlockID::sphere;
			puts("Block: sphere");
		}
		if (input.mouseHeld(0)) {
			if (world.setBlock((V3i)bpos + playerPos.getChunk() * CHUNK_SIZE, {toolBlock}))
				puts("Placed!");
		}
		if (input.mouseHeld(1)) {
			if (world.destroyBlock((V3i)bpos + playerPos.getChunk() * CHUNK_SIZE))
				puts("Destroyed!");
		}
		auto workCounter = getPerformanceCounter();
		auto workSecondsElapsed = getSecondsElapsed(lastCounter, workCounter, counterFrequency);
		if (workSecondsElapsed < targetFrameTime) {
			if (sleepIsAccurate) {
				i32 msToSleep = (i32)((targetFrameTime - workSecondsElapsed) * 1000.0f);
				if (msToSleep > 0) {
					Sleep((DWORD)msToSleep);
				}
			}
			while (workSecondsElapsed < targetFrameTime) {
				workSecondsElapsed = getSecondsElapsed(lastCounter, getPerformanceCounter(), counterFrequency);
			}
		}
		else {
			puts("Low framerate!");
		}
		auto endCounter = getPerformanceCounter();
		f32 frameTime = getSecondsElapsed(lastCounter, endCounter, counterFrequency);
		lastCounter = endCounter;
		DHR(swapChain->Present(0, 0));
	}
	chunkLoader.join();

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