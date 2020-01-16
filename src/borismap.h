#pragma once
#include <stdint.h> 
#include <Windows.h>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <io.h>

#include "common.h"
using FilePos = u64;
namespace BorisHash {

#define HASH_BUCKET_COUNT 65536
#define HASH_ENTRIES_PER_BLOCK 32
#define HASH_PATH DATA "save/hash"

u32 makeHash(V3i id) {
	return std::hash<V3i>{}(id) % HASH_BUCKET_COUNT;
}

FILE* file;
struct Entry {
	V3i worldPos {INVALID_BLOCK_POS,0,0};
	FilePos filePos = 1;
	bool invalid() {
		return worldPos.x == INVALID_BLOCK_POS;
	}
};
struct Block {
	FilePos nextBlock = 0, prevBlock = 0;
	Entry entries[HASH_ENTRIES_PER_BLOCK] {};
};

Block readBlock(FilePos position) {
	_fseeki64(file, position, SEEK_SET);
	Block result;
	assert(fread(&result, sizeof(Block), 1, file) != 0)
	return result;
}

void writeBlock(Block& blk, FilePos position) {
	_fseeki64(file, position, SEEK_SET);
	assert(fwrite(&blk, sizeof(Block), 1, file) != 0);
}

FilePos getFileSize() {
	_fseeki64(file, 0, SEEK_END);
	return _ftelli64(file);
}

void truncateFile(FilePos size) {
	FILE* f = fopen(HASH_PATH, "r+");
	assert(f);
	assert(_chsize_s(fileno(f), size) == 0);
	fclose(f);
}

struct bucket {
	FilePos firstBlockPos = 0, lastBlockPos = 0;
	bool hasStudent(V3i zachetka) {
		FilePos currentBlock = firstBlockPos;
		while (currentBlock) {
			Block searchBlock = {};
			searchBlock = readBlock(currentBlock);
			for (int i = 0; i < HASH_ENTRIES_PER_BLOCK; ++i) {
				if (searchBlock.entries[i].invalid())
					break;
				if (searchBlock.entries[i].worldPos == zachetka)
					return true;
			}
			currentBlock = searchBlock.nextBlock;
		}
		return false;
	}
	void add(Entry& stud) {
		//__debugbreak();//точка останова
		assert(!hasStudent(stud.worldPos));
		if (firstBlockPos == 0) {
			Block newBlock = {};
			newBlock.entries[0] = stud;
			_fseeki64(file, 0, SEEK_END);
			firstBlockPos = lastBlockPos = _ftelli64(file);
			assert(firstBlockPos);
			assert(fwrite(&newBlock, sizeof(Block), 1, file) != 0);
		}
		else {
			Block lastBlock = {};
			lastBlock = readBlock(lastBlockPos);
			int DefPos = 1;
			for (; DefPos < 5; DefPos++)
				if (lastBlock.entries[DefPos].invalid())
					break;
			if (DefPos == 5) {
				lastBlock.nextBlock = getFileSize();
				writeBlock(lastBlock, lastBlockPos);
				lastBlock = {};
				lastBlock.entries[0] = stud;
				lastBlock.prevBlock = lastBlockPos;
				lastBlockPos = getFileSize();
				writeBlock(lastBlock, lastBlockPos);
			}
			else {
				lastBlock.entries[DefPos] = stud;
				writeBlock(lastBlock, lastBlockPos);
			}
		}
	}
	FilePos find(V3i id) {
		FilePos currentBlock = firstBlockPos;
		while (currentBlock) {
			auto searchBlock = readBlock(currentBlock);
			for (int i = 0; i < HASH_ENTRIES_PER_BLOCK; i++) {
				if (searchBlock.entries[i].invalid())
					break;
				if (searchBlock.entries[i].worldPos == id) {
					return searchBlock.entries[i].filePos;
				}
			}
			currentBlock = searchBlock.nextBlock;
		}
		return FILEPOS_NOT_LOADED;
	}
};

struct hashMap {
	bucket buckets[HASH_BUCKET_COUNT];
	void add(Entry& stud) {
		buckets[makeHash(stud.worldPos)].add(stud);
	}
	FilePos find(V3i id) {
		return buckets[makeHash(id)].find(id);
	}
};
hashMap map;
void writeMap() {
	_fseeki64(file, 0, SEEK_SET);
	assert(fwrite(&map, sizeof(hashMap), 1, file) != 0);
}
void init() {
	file = fopen(HASH_PATH, "r+b");
	if (file) {
		assert(fread(&map, sizeof(map), 1, file) != 0);
	} else {
		file = fopen(HASH_PATH, "w+b");
		assert(file);
		assert(fwrite(&map, sizeof(map), 1, file) != 0);
	}
}
void shutdown() {
	fclose(file);
}
void add(Entry entry) {
	map.add(entry);
	writeMap();
}
void debug() {
	for (int i=0; i < HASH_BUCKET_COUNT; ++i) {
		auto& b = map.buckets[i];
		printf("Bucket # %u, firstBlock: %zu, lastBlock: %zu\n", i, b.firstBlockPos, b.lastBlockPos);
	}
	FilePos fileSize = getFileSize();
	FilePos currentBlock = sizeof(hashMap);
	while (currentBlock < fileSize) {
		auto iblock = readBlock(currentBlock);
		printf("position in file: %zu\nprevBlock: %zu\nnextBlock: %zu\n", currentBlock, iblock.prevBlock, iblock.nextBlock);
		for (int i = 0; i < HASH_ENTRIES_PER_BLOCK; ++i) {
			auto& e = iblock.entries[i];
			if (e.invalid())
				break;
			printf("w: ");
			std::cout << e.worldPos;
			printf(", f: %zu\n", e.filePos);
		}
		puts("");
		currentBlock += sizeof(Block);
	}
}
FilePos find(V3i pos) {
	return map.find(pos);
}
}