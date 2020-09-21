#pragma once

#include <atomic>
#include <stdint.h>
#include <utility>
#include <iostream>
#include <deque>
#include <future>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using wchar = wchar_t;

#define ce constexpr
#define ne noexcept
#define nd [[nodiscard]]
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define CONCAT_(x,y) x##y
#define CONCAT(x,y) CONCAT_(x,y)
#define DEFER Deferrer CONCAT(_deferrer, __LINE__) = [&]()

#define assert(x) do{if(!(x)){__debugbreak();exit(-1);}}while(0)
#ifdef BUILD_RELEASE
#define assert_dbg(x) 
#else
#define assert_dbg(x) assert(x)
#endif

#define STBI_ASSERT assert

auto _fread(void* buf, size_t size, size_t count, FILE* file) ne {
	_set_errno(0);
	auto r = fread(buf, size, count, file);
	assert(r);
	if (errno) {
		printf("fread: %s\n", strerror(errno));
		assert(0);
	}
	return r;
}
auto _fwrite(const void* buf, size_t size, size_t count, FILE* file) ne {
	_set_errno(0);
	auto r = fwrite(buf, size, count, file);
	assert(r);
	if (errno) {
		printf("fwrite: %s\n", strerror(errno));
		assert(0);
	}
	return r;
}
#define fread _fread
#define fwrite _fwrite

nd ce char tohex(u8 val) ne {
	if (val < 10)
		return '0' + val;
	return 'A' + (val - 10);
}

template<class Fn>
struct Deferrer {
	Fn fn;
	Deferrer(Fn&& fn) : fn(fn) {}
	~Deferrer() { fn(); }
};
nd FILE* openFileRW(char const* path) ne {
	auto file = fopen(path, "r+b");
	if (!file)
		return fopen(path, "w+b");
	return file;
}

template<class T>
ce void minmax(const T& a, const T& b, T& omin, T& omax) ne(ne(a < b)) {
	if (a < b) {
		omin = a;
		omax = b;
	}
	else {
		omin = b;
		omax = a;
	}
};

template<class T>
nd ce std::pair<T, T> minmax(const T& a, const T& b) ne(ne(a < b)) {
	if (a < b) {
		return {a, b};
	}
	else {
		return {b, a};
	}
};

#define KBYTES(x) (x*1024)
#define MBYTES(x) (KBYTES(x)*1024)
#define GBYTES(x) (MBYTES(x)*1024)

template<class ByteType = size_t, class Input = ByteType>
std::pair<ByteType, const char*> normalizeBytes(Input input) {
	auto units = "B";
	auto bytes = (ByteType)input;
	if (bytes >= 1024) { bytes /= 1024; units = "KB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "MB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "GB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "TB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "PB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "EB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "ZB"; }
	if (bytes >= 1024) { bytes /= 1024; units = "YB"; }
	return {bytes, units};
}

template<class Container, class Elem>
bool contains(const Container& cont, const Elem& elem) {
	return std::find(std::begin(cont), std::end(cont), elem) != std::end(cont);
}
template<class Vert, size_t size>
ce auto triangulateQuads(const std::array<Vert, size>& verts) ne {
	static_assert(size % 4 == 0);
	std::array<Vert, size / 2 * 3> result;
	Vert* dest = result.data();
	for (int i=0; i < verts.size(); i+=4) {
		*dest++ = verts.begin()[i + 0];
		*dest++ = verts.begin()[i + 1];
		*dest++ = verts.begin()[i + 2];
		*dest++ = verts.begin()[i + 1];
		*dest++ = verts.begin()[i + 3];
		*dest++ = verts.begin()[i + 2];
	}
	return result;
}
nd u32 invertRGBA(u32 c) {
	u32 r = (0xFF - ((c >> 24) & 0xFF)) << 24;
	u32 g = (0xFF - ((c >> 16) & 0xFF)) << 16;
	u32 b = (0xFF - ((c >> 8 ) & 0xFF)) << 8 ;
	u32 a = (0xFF - ((c >> 0 ) & 0xFF)) << 0 ;
	return r | g | b | a;
}
template<class T, size_t blockCapacity>
struct Arena {
	struct Ptr {
		T* data;
		size_t index;
	};
	struct Block {
		struct alignas(alignof(T)) Holder {
			u8 mem[sizeof(T)];
		};
		static_assert(sizeof(Holder) == sizeof(T));
		static_assert(alignof(Holder) == alignof(T));
		bool occupied[blockCapacity];
		Holder data[blockCapacity];
		Block* next;
		Block() : occupied(), next(0) {
		}
		void* allocate(size_t& index) {
			for (u32 i = 0; i < blockCapacity; ++i) {
				if (!occupied[i]) {
					occupied[i] = true;
					return data + i;
				}
				++index;
			}
			return 0;
		}
		bool owns(T* t) {
			return t >= (T*)data && t < (T*)data + blockCapacity;
		}
		void free(T* t) {
			assert(owns(t));
			t->~T();
			size_t idx = t - (T*)data;
			assert(occupied[idx]);
			occupied[idx] = false;
		}
	};
	Arena() {
		first = new Block;
		last = first;
	}
	template<class... Args>
	Ptr allocate(Args&&... args) {
#ifndef BUILD_RELEASE
		++occupiedCount;
#endif
		Block* curr = first;
		void* result = 0;
		size_t index = 0;
		while (curr) {
			result = curr->allocate(index);
			if (result) {
				break;
			}
			result = 0;
			curr = curr->next;
		}
		if (!result)
			result = allocateBlock()->allocate(index);
		return {new(result) T(std::forward<Args>(args)...), index};
	}
	void free(T* ptr) {
		Block* curr = first;
		while (curr) {
			if (curr->owns(ptr)) {
				free(curr, ptr);
				return;
			}
			curr = curr->next;
		}
		assert(!"Bad free param");
	}
	void freeIdx(size_t idx) {
		Block* curr = first;
		while (idx >= blockCapacity) {
			idx -= blockCapacity;
			curr = curr->next;
			assert(curr);
		}
		free(curr, *(T*)(curr->data + idx));
	}
	T& operator[](size_t idx) {
		Block* curr = first;
		while (idx >= blockCapacity) {
			idx -= blockCapacity;
			curr = curr->next;
			assert(curr);
		}
		return *(T*)(curr->data + idx);
	}
	template<class Fn>
	void for_each(Fn&& fn) {
		Block* curr = first;
		while (curr) {
			for (size_t i = 0; i < blockCapacity; ++i) {
				if (curr->occupied[i]) {
					fn((T*)(curr->data + i));
				}
			}
			curr = curr->next;
		}
	}
#ifndef BUILD_RELEASE
	size_t occupiedCount = 0;
#endif
private:
	Block* first = 0;
	Block* last = 0;
	Block* allocateBlock() {
		Block* newBlock = new Block;
		last->next = newBlock;
		last = newBlock;
		return newBlock;
	}
	void free(Block* block, T* ptr) {
#ifndef BUILD_RELEASE
		--occupiedCount;
#endif
		block->free(ptr);
	}
};

template<class T>
struct FutureQueue : private std::deque<std::future<T>> {
	using Base = std::deque<std::future<T>>;
	void push(std::future<T>&& f) {
		Base::push_back(std::move(f));
	}
	void push(const std::future<T>& f) {
		Base::push_back(f);
	}
	T pop() {
		auto fut = std::move(Base::front());
		Base::pop_front();
		return fut.get();
	}
private:
};