#pragma once
#include <stdint.h>
#include <utility>
#define assert(x) if(!(x)) __debugbreak()

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

template<class T>
void swap(T& a, T& b) {
	T t = std::move(a);
	a = std::move(b);
	b = std::move(t);
}

char tohex(u8 val) {
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
#define CONCAT(x,y) x##y
#define CONCAT2(x,y) CONCAT(x,y)
#define DEFER Deferrer CONCAT2(_deferrer, __LINE__) = [&]()

FILE* openFileRW(char const* path) {
	auto file = fopen(path, "r+b");
	if (!file)
		return fopen(path, "w+b");
	return file;
}