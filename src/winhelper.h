#pragma once
#define NOMINMAX
#include <Windows.h>
struct WndClassA final : WNDCLASSA {
	WndClassA() : WNDCLASSA() {}
	void checkIn() {
		RegisterClassA(this);
	}
};
struct WndClassW final : WNDCLASSW {
	WndClassW() : WNDCLASSW() {}
	void checkIn() {
		RegisterClassW(this);
	}
};
struct WndClassExA final : WNDCLASSEXA {
	WndClassExA() : WNDCLASSEXA() {
		cbSize = sizeof(WNDCLASSEXA);
	}
	void checkIn() {
		RegisterClassExA(this);
	}
};
struct WndClassExW final : WNDCLASSEXW {
	WndClassExW() : WNDCLASSEXW() {
		cbSize = sizeof(WNDCLASSEXW);
	}
	void checkIn() {
		RegisterClassExW(this);
	}
};
#ifdef UNICODE
using WndClass = WndClassW;
using WndClassEx = WndClassExW;
#else
using WndClass = WndClassA;
using WndClassEx = WndClassExA;
#endif
struct Rect : RECT {
	auto width() const { return right - left; }
	auto height() const { return bottom - top; }
	void xywh(LONG x, LONG y, LONG w, LONG h) {
		left = x;
		top = y;
		right = x + w;
		bottom = y + h;
	}
};
i64 getPerformanceCounter() {
	LARGE_INTEGER Counter;
	QueryPerformanceCounter(&Counter);
	return Counter.QuadPart;
}
f32 getSecondsElapsed(i64 begin, i64 end, i64 freq) {
	return (f32)(end - begin) / (f32)freq;
}
struct TimedScope {
	const char* name;
	LARGE_INTEGER begin;
	TimedScope(const char* name) : name(name) {
		QueryPerformanceCounter(&begin);
	}
	~TimedScope() {
		LARGE_INTEGER end, freq;
		QueryPerformanceCounter(&end);
		QueryPerformanceFrequency(&freq);
		printf("%-20s: %.2f ms\n", name, (end.QuadPart - begin.QuadPart) * 1000 / (f32)freq.QuadPart);
	}
};
#ifdef BUILD_RELEASE
#define TIMED_SCOPE(name)
#define TIMED_FUNCTION 
#else
#define TIMED_SCOPE(name) TimedScope _timedScope(name);
#define TIMED_FUNCTION TimedScope _timedFunction(__FUNCTION__);
#endif
#define TIMED_SCOPE_(name) TimedScope _timedScope(name);
#define TIMED_FUNCTION_ TimedScope _timedFunction(__FUNCTION__);
