#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
namespace WH {
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
i64 getPerformanceFrequency() {
	LARGE_INTEGER Counter;
	QueryPerformanceFrequency(&Counter);
	return Counter.QuadPart;
}
f32 getSecondsElapsed(i64 begin, i64 end, i64 freq) {
	return (f32)(end - begin) / (f32)freq;
}
struct TimedScope {
	const char* name;
	i64 begin;
	TimedScope(const char* name) : name(name), begin(getPerformanceCounter()) {
	}
	~TimedScope() {
		printf("%s: %.2f ms\n", name, (getPerformanceCounter() - begin) * 1000 / (f32)getPerformanceFrequency());
	}
};
#ifdef BUILD_RELEASE
#define TIMED_SCOPE(name)
#define TIMED_FUNCTION 
#else
#define TIMED_SCOPE(name)  ::WH::TimedScope _timedScope(name);
#define TIMED_FUNCTION     ::WH::TimedScope _timedFunction(__FUNCTION__);
#endif
#define TIMED_SCOPE_(name) ::WH::TimedScope _timedScope(name);
#define TIMED_FUNCTION_    ::WH::TimedScope _timedFunction(__FUNCTION__);
HWND getWallpaperWindow() {
	auto progman = FindWindowA("Progman", 0);
	if (!progman) 
		return 0;
	if (!SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, 0))
		return 0;
	HWND result = 0;
	EnumWindows([](HWND topwnd, LPARAM result) -> BOOL {
		if (FindWindowExA(topwnd, 0, "SHELLDLL_DefView", 0))
			*(HWND*)result = FindWindowExA(0, topwnd, "WorkerW", 0);
		return true;
	}, (LPARAM)&result);
	return result;
}
}