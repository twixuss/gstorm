#pragma once
#include <stdint.h>
#include <utility>
#include <iostream>

#define assert(x) do{if(!(x)){__debugbreak();exit(-1);}}while(0)
#ifdef BUILD_RELEASE
#define assert_dbg(x) 
#else
#define assert_dbg(x) assert(x)
#endif

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
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define CONCAT(x,y) x##y
#define CONCAT2(x,y) CONCAT(x,y)
#define DEFER Deferrer CONCAT2(_deferrer, __LINE__) = [&]()

FILE* openFileRW(char const* path) {
	auto file = fopen(path, "r+b");
	if (!file)
		return fopen(path, "w+b");
	return file;
}

template<class ...T>
void Print(const T&... t) {
	int dummy[] = {(std::cout << t, 0)...};
}
template<class T>
void minmax(T a, T b, T& omin, T& omax) {
	if (a < b) {
		omin = a;
		omax = b;
	}
	else {
		omin = b;
		omax = a;
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