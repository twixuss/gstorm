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
#define HASH_ENTRIES_PER_BLOCK 16
#define HASH_PATH DATA "save/hash"

u32 makeHash(V3i key) {
	return std::hash<V3i>{}(key) % HASH_BUCKET_COUNT;
}

FILE* file;
#pragma pack(push, 1)
struct Entry {
	V3i key {0,0,0};
	FilePos filePos = FILEPOS_INVALID;
	bool invalid() {
		//return key.x == INT_MIN;
		return filePos == FILEPOS_INVALID;
	}
};
#pragma pack(pop)
struct Block {
	FilePos nextBlock = 0;
#if HASH_ENTRIES_PER_BLOCK == 1
	Entry entry {};
#else
	Entry entries[HASH_ENTRIES_PER_BLOCK] {};
#endif
};

Block readBlock(FilePos position) {
	_fseeki64(file, position, SEEK_SET);
	Block result;
	assert(fread(&result, sizeof(Block), 1, file) != 0);
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
struct Bucket {
	FilePos firstBlockPos = 0, lastBlockPos = 0;
	FilePos find(V3i key) {
		FilePos currentBlockPos = firstBlockPos;
		while (currentBlockPos) {
			auto currentBlock = readBlock(currentBlockPos);
			for (int i = 0; i < HASH_ENTRIES_PER_BLOCK; i++) {
				if (currentBlock.entries[i].invalid())
					break;
				if (currentBlock.entries[i].key == key) 
					return currentBlock.entries[i].filePos;
			}
			currentBlockPos = currentBlock.nextBlock;
		}
		return FILEPOS_INVALID;
	}
	void add(Entry& entry) {
		auto duplicate = find(entry.key);
		assert(duplicate == FILEPOS_INVALID); 
		if (firstBlockPos == 0) {
			Block newBlock = {};
			newBlock.entries[0] = entry;
			firstBlockPos = lastBlockPos = getFileSize();
			writeBlock(newBlock, firstBlockPos);
		}
		else {
			Block lastBlock = readBlock(lastBlockPos);
			int targetIdx = 1;
			for (; targetIdx < HASH_ENTRIES_PER_BLOCK; targetIdx++) {
				if (lastBlock.entries[targetIdx].invalid()) {
					lastBlock.entries[targetIdx] = entry;
					writeBlock(lastBlock, lastBlockPos);
					return;
				}
			}
			auto prelastBlockPos = lastBlockPos;
			auto prelastBlock = readBlock(prelastBlockPos);
			prelastBlock.nextBlock = getFileSize();

			lastBlockPos = prelastBlock.nextBlock;
			lastBlock = {};
			lastBlock.entries[0] = entry;

			writeBlock(prelastBlock, prelastBlockPos);
			writeBlock(lastBlock, lastBlockPos);
		}
	}
};

struct Map {
	Bucket buckets[HASH_BUCKET_COUNT];
	void add(Entry& entry) {
		buckets[makeHash(entry.key)].add(entry);
	}
	FilePos find(V3i key) {
		return buckets[makeHash(key)].find(key);
	}
};
Map map;
void writeMap() {
	_fseeki64(file, 0, SEEK_SET);
	assert(fwrite(&map, sizeof(Map), 1, file) != 0);
}
void init() {
	file = fopen(HASH_PATH, "r+b");
	//file = 0; // !!!!!!!!!!!!!!!!!!!!!!!!!!!!! TODO DEBUG
	if (file) {
		assert(fread(&map, sizeof(Map), 1, file) != 0);
	}
	else {
		file = fopen(HASH_PATH, "w+b");
		assert(file);
		assert(fwrite(&map, sizeof(Map), 1, file) != 0);
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
	FilePos currentBlock = sizeof(Map);
	while (currentBlock < fileSize) {
		auto iblock = readBlock(currentBlock);
		printf("position in file: %llu\nnextBlock: %llu\n", currentBlock, iblock.nextBlock);
		for (int i = 0; i < HASH_ENTRIES_PER_BLOCK; ++i) {
			auto& e = iblock.entries[i];
			if (e.invalid())
				break;
			printf("w: ");
			std::cout << e.key;
			printf(", f: %llu\n", e.filePos);
		}
		puts("");
		currentBlock += sizeof(Block);
	}
}
FilePos find(V3i key) {
	return map.find(key);
}
}