#pragma once
#include "common.h"
#define PI 3.141592653f
#define PI2 (PI * 2.0f)
#define INVPI (1.0f / PI)
#define INVPI2 (1.0f / PI2)
#define DEG2RAD(deg) ((deg) / 360.0f * PI2)
#define RAD2DEG(rad) ((rad) / PI2 * 360.0f)

#define ROOT2 1.414213538f
#define ROOT3 1.732050776f

struct V2i;
struct V3i;
struct V4i;

struct V2 {
	f32 x = 0;
	f32 y = 0;
	constexpr V2() = default;
	constexpr V2(f32 x, f32 y) : x(x), y(y) {}
	constexpr V2(f32 v) : V2(v, v) {}
	template<class T>
	constexpr explicit V2(T x, T y) : V2((f32)x, (f32)y) {}
	constexpr explicit V2(V2i b);
	constexpr V2 operator-() const { return {-x,-y}; }
	constexpr V2 operator+(V2 b) const { return {x + b.x,y + b.y}; }
	constexpr V2 operator-(V2 b) const { return {x - b.x,y - b.y}; }
	constexpr V2 operator*(V2 b) const { return {x * b.x,y * b.y}; }
	constexpr V2 operator/(V2 b) const { return {x / b.x,y / b.y}; }
	constexpr V2& operator+=(V2 b) { *this = *this + b; return *this; }
	constexpr V2& operator-=(V2 b) { *this = *this - b; return *this; }
	constexpr V2& operator*=(V2 b) { *this = *this * b; return *this; }
	constexpr V2& operator/=(V2 b) { *this = *this / b; return *this; }
	constexpr bool operator==(V2 b) const { return x == b.x && y == b.y; }
	constexpr bool operator!=(V2 b) const { return !(*this == b); }
	constexpr f32 dot(V2 b) const {
		return x * b.x + y * b.y;
	}
	constexpr f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	constexpr f32* data() { return &x; }
	constexpr const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream& os, V2 v) {
		return os << v.x << ", " << v.y;
	}
};
struct V3 {
	f32 x = 0;
	f32 y = 0;
	f32 z = 0;
	constexpr V3() = default;
	constexpr V3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
	constexpr V3(f32 v) : V3(v, v, v) {}
	constexpr explicit V3(V3i b);
	constexpr V3 operator-() const { return {-x,-y,-z}; }
	constexpr V3 operator+(V3 b) const { return {x + b.x,y + b.y,z + b.z}; }
	constexpr V3 operator-(V3 b) const { return {x - b.x,y - b.y,z - b.z}; }
	constexpr V3 operator*(V3 b) const { return {x * b.x,y * b.y,z * b.z}; }
	constexpr V3 operator/(V3 b) const { return {x / b.x,y / b.y,z / b.z}; }
	constexpr V3& operator+=(V3 b) { *this = *this + b; return *this; }
	constexpr V3& operator-=(V3 b) { *this = *this - b; return *this; }
	constexpr V3& operator*=(V3 b) { *this = *this * b; return *this; }
	constexpr V3& operator/=(V3 b) { *this = *this / b; return *this; }
	constexpr bool operator==(V3 b) const { return x == b.x && y == b.y && z == b.z; }
	constexpr bool operator!=(V3 b) const { return !(*this == b); }
	constexpr f32 dot(V3 b) const {
		return x * b.x + y * b.y + z * b.z;
	}
	constexpr f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	constexpr V2 xz() const {
		return {x, z};
	}
	constexpr V3 cross(V3 b) const {
		return {
		   y * b.z - z * b.y,
		   z * b.x - x * b.z,
		   x * b.y - y * b.x
		};
	}
	V3i rounded() const;
	V3 normalized() const {
		return *this * (1.0f / length());
	}
	constexpr f32* data() { return &x; }
	constexpr const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream& os, V3 v) {
		return os << v.x << ", " << v.y << ", " << v.z;
	}
};
struct alignas(16) V4 {
	f32 x = 0;
	f32 y = 0;
	f32 z = 0;
	f32 w = 0;
	V4() = default;
	V4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
	V4(f32 v) : V4(v, v, v, v) {}
	explicit V4(V4i b);
	V4 operator+(V4 b) const { return {x + b.x,y + b.y,z + b.z,w + b.w}; }
	V4 operator-(V4 b) const { return {x - b.x,y - b.y,z - b.z,w - b.w}; }
	V4 operator*(V4 b) const { return {x * b.x,y * b.y,z * b.z,w * b.w}; }
	V4 operator/(V4 b) const { return {x / b.x,y / b.y,z / b.z,w / b.w}; }
	V4& operator+=(V4 b) { *this = *this + b; return *this; }
	V4& operator-=(V4 b) { *this = *this - b; return *this; }
	V4& operator*=(V4 b) { *this = *this * b; return *this; }
	V4& operator/=(V4 b) { *this = *this / b; return *this; }
	bool operator==(V4 b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
	bool operator!=(V4 b) const { return !(*this == b); }
	V4 operator-() {
		return {-x,-y,-z,-w};
	}
	constexpr f32* data() { return &x; }
	constexpr const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream& os, V4 v) {
		return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
	}
	f32 dot(V4 b) const {
		return x * b.x + y * b.y + z * b.z + w * b.w;
	}
	f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
};
struct V2i {
	i32 x = 0;
	i32 y = 0;
	V2i() = default;
	V2i(i32 x, i32 y) : x(x), y(y) {}
	V2i(i32 v) : V2i(v, v) {}
	V2i operator-() const { return {-x,-y}; }
	V2i operator+(V2i b) const { return {x + b.x,y + b.y}; }
	V2i operator-(V2i b) const { return {x - b.x,y - b.y}; }
	V2i operator*(V2i b) const { return {x * b.x,y * b.y}; }
	V2i operator/(V2i b) const { return {x / b.x,y / b.y}; }
	V2i& operator+=(V2i b) { *this = *this + b; return *this; }
	V2i& operator-=(V2i b) { *this = *this - b; return *this; }
	V2i& operator*=(V2i b) { *this = *this * b; return *this; }
	V2i& operator/=(V2i b) { *this = *this / b; return *this; }
	bool operator==(V2i b) const { return x == b.x && y == b.y; }
	bool operator!=(V2i b) const { return !(*this == b); }
	friend std::ostream& operator<<(std::ostream& os, V2i v) {
		return os << v.x << ", " << v.y;
	}
};
struct V3i {
	i32 x = 0;
	i32 y = 0;
	i32 z = 0;
	V3i() = default;
	V3i(i32 x, i32 y, i32 z) : x(x), y(y), z(z) {}
	V3i(i32 v) : V3i(v, v, v) {}
	explicit V3i(V3 b);
	V3i operator-() const { return {-x,-y,-z}; }
	V3i operator+(V3i b) const { return {x + b.x,y + b.y,z + b.z}; }
	V3i operator-(V3i b) const { return {x - b.x,y - b.y,z - b.z}; }
	V3i operator*(V3i b) const { return {x * b.x,y * b.y,z * b.z}; }
	V3i operator/(V3i b) const { return {x / b.x,y / b.y,z / b.z}; }
	V3i& operator+=(V3i b) { *this = *this + b; return *this; }
	V3i& operator-=(V3i b) { *this = *this - b; return *this; }
	V3i& operator*=(V3i b) { *this = *this * b; return *this; }
	V3i& operator/=(V3i b) { *this = *this / b; return *this; }
	bool operator==(V3i b) const { return x == b.x && y == b.y && z == b.z; }
	bool operator!=(V3i b) const { return !(*this == b); }
	V3i absolute() const {
		return {
			labs(x),
			labs(y),
			labs(z)
		};
	}
	friend std::ostream& operator<<(std::ostream& os, V3i v) {
		return os << v.x << ", " << v.y << ", " << v.z;
	}
};
struct V4i {
	i32 x = 0;
	i32 y = 0;
	i32 z = 0;
	i32 w = 0;
	V4i() = default;
	V4i(i32 x, i32 y, i32 z, i32 w) : x(x), y(y), z(z), w(w) {}
	V4i(i32 v) : V4i(v, v, v, v) {}
	explicit V4i(V4 b);
	V4i operator-() const { return {-x,-y,-z,-w}; }
	V4i operator+(V4i b) const { return {x + b.x,y + b.y,z + b.z,w + b.w}; }
	V4i operator-(V4i b) const { return {x - b.x,y - b.y,z - b.z,w - b.w}; }
	V4i operator*(V4i b) const { return {x * b.x,y * b.y,z * b.z,w * b.w}; }
	V4i operator/(V4i b) const { return {x / b.x,y / b.y,z / b.z,w / b.w}; }
	V4i& operator+=(V4i b) { *this = *this + b; return *this; }
	V4i& operator-=(V4i b) { *this = *this - b; return *this; }
	V4i& operator*=(V4i b) { *this = *this * b; return *this; }
	V4i& operator/=(V4i b) { *this = *this / b; return *this; }
	bool operator==(V4i b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
	bool operator!=(V4i b) const { return !(*this == b); }
	V4i absolute() const {
		return {
			labs(x),
			labs(y),
			labs(z),
			labs(w),
		};
	}
	friend std::ostream& operator<<(std::ostream& os, V4i v) {
		return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
	}
};
struct alignas(64) M4 {
	union {
		struct {
			V4 i, j, k, l;
		};
		f32 m[16];
	};
	M4() : m() {}
	M4(V4 i, V4 j, V4 k, V4 l) : i(i), j(j), k(k), l(l) {}
	M4(f32 ix, f32 iy, f32 iz, f32 iw,
	   f32 jx, f32 jy, f32 jz, f32 jw,
	   f32 kx, f32 ky, f32 kz, f32 kw,
	   f32 lx, f32 ly, f32 lz, f32 lw) :
		i(ix, iy, iz, iw),
		j(jx, jy, jz, jw),
		k(kx, ky, kz, kw),
		l(lx, ly, lz, lw) {
	}
	V4 operator*(V4 b) const {
		return {
			i.x * b.x + j.x * b.y + k.x * b.z + l.x * b.w,
			i.y * b.x + j.y * b.y + k.y * b.z + l.y * b.w,
			i.z * b.x + j.z * b.y + k.z * b.z + l.z * b.w,
			i.w * b.x + j.w * b.y + k.w * b.z + l.w * b.w,
		};
	}
	V3 operator*(V3 b) const {
		return {
			i.x * b.x + j.x * b.y + k.x * b.z,
			i.y * b.x + j.y * b.y + k.y * b.z,
			i.z * b.x + j.z * b.y + k.z * b.z,
		};
	}
	M4 operator*(M4 b) const {
		return {
			*this * b.i,
			*this * b.j,
			*this * b.k,
			*this * b.l
		};
	}
	M4& operator*=(M4 b) {
		*this = *this * b;
		return *this;
	}
	static M4 scaling(V3 v) {
		return {
			v.x, 0, 0, 0,
			0, v.y, 0, 0,
			0, 0, v.z, 0,
			0, 0, 0, 1
		};
	}
	static M4 translation(V3 v) {
		return {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			v.x, v.y, v.z, 1,
		};
	}
	static M4 projection(f32 aspect, f32 fov, f32 nz, f32 fz) {
		f32 h = 1.0f / tanf(fov * 0.5f);
		f32 w = h / aspect;
		f32 fzdfmn = fz / (fz - nz);
		return {
			w, 0, 0, 0,
			0, h, 0, 0,
			0, 0, fzdfmn, 1,
			0, 0, -fzdfmn * nz, 0
		};
	}
	static M4 rotationX(f32 a) {
		return
		{
			1, 0,        0,       0,
			0, cosf(a),  sinf(a), 0,
			0, -sinf(a), cosf(a), 0,
			0, 0,        0,       1,
		};
	}
	static M4 rotationY(f32 a) {
		return
		{
			cosf(a),  0, sinf(a), 0,
			0,        1, 0,       0,
			-sinf(a), 0, cosf(a), 0,
			0,        0, 0,       1,
		};
	}
	static M4 rotationZ(f32 a) {
		return
		{
			cosf(a),  sinf(a), 0, 0,
			-sinf(a), cosf(a), 0, 0,
			0,        0,       1, 0,
			0,        0,       0, 1
		};
	}
	//Roll, Pitch, Yaw (ZXY)
	//Roll, Pitch, Yaw (ZXY)
	static M4 rotationZXY(V3 v) {
		return (rotationY(v.y) * rotationX(v.x)) * rotationZ(v.z);
	}
	//Yaw, Pitch, Roll (YXZ)
	//Yaw, Pitch, Roll (YXZ)
	static M4 rotationYXZ(V3 v) {
		return (rotationZ(v.z) * rotationX(v.x)) * rotationY(v.y);
	}
	friend std::ostream& operator<<(std::ostream& os, M4 m) {
		return os << m.i << ", " << m.j << ", " << m.k << ", " << m.l;
	}
};
u8 randomU8(u8 r) {
	r += 0x0C;
	r *= 0x61;
	r ^= 0xB2;
	r -= 0x80;
	r ^= 0xF5;
	r *= 0xA7;
	return (r << 4) | (r >> 4);
}
u32 randomU32(u32 r) {
	r += 0x0C252DA0;
	r *= 0x55555561;
	r ^= 0xB23E2387;
	r -= 0x8069BAC0;
	r ^= 0xF5605798;
	r *= 0xAAAAAABF;
	return _rotl(r, 16);
}
u32 randomU32(i32 in) {
	return randomU32((u32)in);
}
u32 randomU32(V3i in) {
	auto x = randomU32(in.x);
	auto y = randomU32(in.y);
	auto z = randomU32(in.z);
	return x + y + z;
}
u64 randomU64(V3i in) {
	auto x = randomU32(in.x);
	auto y = randomU32(in.y);
	auto z = randomU32(in.z);
	return 
		(u64)x | ((u64)y << 32) +
		(u64)z | ((u64)x << 32) +
		(u64)y | ((u64)z << 32);
}
#ifdef _M_AMD64
#define randomSize(x) randomU64(x)
#elif defined(_M_IX86)
#define randomSize(x) randomU32(x)
#else
#error Unknown architecture
#endif
namespace std {
template<>
struct hash<V3> {
	size_t operator()(V3 val) const {
		return
			hash<f32>()(val.x) ^
			hash<f32>()(val.y) ^
			hash<f32>()(val.z);
	}
};
template<>
struct hash<V3i> {
	size_t operator()(V3i val) const {
		return
			hash<i32>()(val.x) ^
			hash<i32>()(val.y) ^
			hash<i32>()(val.z);
	}
};
}
f32 frac(f32 x) {
	auto r = x - (i64)x;
	if (r < 0)
		++r;
	return r;
}
V2 frac(V2 v) {
	return {
		frac(v.x),
		frac(v.y),
	};
}
V3 frac(V3 v) {
	return {
		frac(v.x),
		frac(v.y),
		frac(v.z),
	};
}
V4 frac(V4 v) {
	return {
		frac(v.x),
		frac(v.y),
		frac(v.z),
		frac(v.w),
	};
}
int frac(int v, int s) {
	v %= s;
	if (v < 0)
		v += s;
	return v;
}
V2 floor(V2 v) {
	return {
		floorf(v.x),
		floorf(v.y),
	};
}
V3 floor(V3 v) {
	return {
		floorf(v.x),
		floorf(v.y),
		floorf(v.z),
	};
}
V4 floor(V4 v) {
	return {
		floorf(v.x),
		floorf(v.y),
		floorf(v.z),
		floorf(v.w),
	};
}
int floor(int v, int step) {
	if (v < 0 && v % step != 0)
		return v / step * step - step;
	else 
		return v / step * step;
}
V2i floor(V2i v, int step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
	};
}
V3i floor(V3i v, int step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
		floor(v.z, step),
	};
}
V4i floor(V4i v, int step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
		floor(v.z, step),
		floor(v.w, step),
	};
}
V2i frac(V2i v, int s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
	};
}
V3i frac(V3i v, int s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
		frac(v.z, s),
	};
}
V4i frac(V4i v, int s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
		frac(v.z, s),
		frac(v.w, s),
	};
}
f32 dot(V2 a, V2 b) { return a.dot(b); }
f32 dot(V3 a, V3 b) { return a.dot(b); }
f32 dot(V4 a, V4 b) { return a.dot(b); }
f32 distanceSqr(V2 a, V2 b) { return (a - b).lengthSqr(); }
f32 distanceSqr(V3 a, V3 b) { return (a - b).lengthSqr(); }
f32 distanceSqr(V4 a, V4 b) { return (a - b).lengthSqr(); }
f32 distance(V2 a, V2 b) { return sqrt(distanceSqr(a, b)); }
f32 distance(V3 a, V3 b) { return sqrt(distanceSqr(a, b)); }
f32 distance(V4 a, V4 b) { return sqrt(distanceSqr(a, b)); }
template<class T>
T min(T a, T b) {
	return a < b ? a : b;
}
template<class T>
T max(T a, T b) {
	return a > b ? a : b;
}
template<class T>
T clamp(T a, T mi, T ma) {
	return min(max(a, mi), ma);
}
template<class T>
T lerp(T a, T b, f32 t) {
	return a + (b - a) * t;
}
f32 coserp(f32 a, f32 b, f32 t) {
	auto ft = t * PI;
	auto f = (1 - cosf(ft)) * 0.5f;
	return a * (1 - f) + b * f;
}
f32 noise(i32 x) {
	x = (x << 13) ^ x;
	return (1.0f - ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}
f32 noise(i32 x, i32 y) {
	auto n = x + y * 57;
	n = (n << 13) ^ n;
	return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 2147483648.0f);
}
f32 cosNoise(f32 v) {
	auto fl = (int)floor(v);
	auto l = noise(fl);
	auto r = noise(fl + 1);
	return coserp(l, r, frac(v));
}
f32 cosNoise(V2 p) {
	auto x = (int)floor(p.x);
	auto y = (int)floor(p.y);
	auto bl = noise(x,     y);
	auto tl = noise(x,     y + 1);
	auto br = noise(x + 1, y);
	auto tr = noise(x + 1, y + 1);
	auto fr = frac(p.x);
	auto b = coserp(bl, br, fr);
	auto t = coserp(tl, tr, fr);
	return coserp(b, t, frac(p.y));
}
f32 perlin(V2 v, u32 o) {
	f32 result = 0.0f;
	f32 div = 0.0f;
	f32 vmul = 1.0f;
	f32 pmul = 1.0f;
	for (u32 i = 0; i < o; ++i) {
		result += cosNoise(v * pmul) * vmul;
		div += vmul;
		vmul *= 0.5f;
		pmul *= 2.0f;
	}
	return result / div;
};
V2 random01(V2 p) {
	V2 a = frac((p + ROOT2) * V2 {PI, PI * 2});
	a += dot(a, a + PI * 4);
	return frac(V2 {a.x * a.y, a.x + a.y});
};
V3 random01(V3 p) {
	V3 a = frac((p + ROOT2) * V3 {PI, PI * 2, PI * 3});
	a += dot(a, a + PI * 4);
	return frac(V3 {a.x * a.y, a.y * a.z, a.x * a.z});
};
f32 voronoi(V2 v) {
	V2 rel = frac(v) - 0.5f;
	V2 tile = floor(v);
	f32 minDist = 1000;
	auto getPos = [tile](V2 off) { return random01(tile + off) + off - 0.5f; };
#if 1
	// 10.5 s
	for (f32 x = -1; x <= 1; ++x) {
		minDist = min(minDist, distanceSqr(rel, getPos(V2 {x,-1})));
		minDist = min(minDist, distanceSqr(rel, getPos(V2 {x, 0})));
		minDist = min(minDist, distanceSqr(rel, getPos(V2 {x, 1})));
	}
#endif
#if 0
	// 13 s
	for (i32 x = -1; x <= 1; ++x) {
		for (i32 y = -1; y <= 1; ++y) {
			V2 off {(f32)x, (f32)y};
			minDist = min(minDist, distanceSqr(rel, random01(tile + off) + off - 0.5f));
		}
	}
#endif
#if 0
	// 17 s
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {-1,-1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {-1, 0})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {-1, 1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 0,-1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 0, 0})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 0, 1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 1,-1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 1, 0})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 { 1, 1})));
#endif
	return sqrt(minDist) * (1 / ROOT2);
}
f32 voronoi(V3 v) {
	V3 rel = frac(v) - 0.5f;
	V3 tile = floor(v);
	f32 minDist = 1000;
	for (i32 x = -1; x <= 1; ++x) {
		for (i32 y = -1; y <= 1; ++y) {
			for (i32 z = -1; z <= 1; ++z) {
				V3 off {(f32)x, (f32)y, (f32)z};
				minDist = min(minDist, distanceSqr(rel, random01(tile + off) + off - 0.5f));
			}
		}
	}
	return sqrt(minDist) * (1 / ROOT3);
}
f32 voronoiCrackle(V2 v) {
	V2 rel = frac(v) - 0.5f;
	V2 tile = floor(v);
	f32 closestPoints[3] {8,8,8};
	for (i32 x = -2; x <= 2; ++x) {
		for (i32 y = -2; y <= 2; ++y) {
			V2 off {(f32)x, (f32)y};
			auto dist = distanceSqr(rel, off + random01(tile + off) - 0.5f);
			float maxClosestDist = 0.;
			int maxClosestDistIdx = 0;
			for (int i=0; i < _countof(closestPoints); ++i) {
				if (closestPoints[i] > maxClosestDist) {
					maxClosestDist = closestPoints[i];
					maxClosestDistIdx = i;
				}
			}
			if (dist < maxClosestDist) {
				closestPoints[maxClosestDistIdx] = dist;
			}
		}
	}
	return sqrt(max(max(closestPoints[0], closestPoints[1]), closestPoints[2])) * (1 / ROOT2);
}
template<class Seed, class FnRet>
FnRet textureDetail(u32 octaves, Seed seed, FnRet (&&fn)(Seed), f32 persistence = 0.5f) {
	FnRet result {};
	f32 vmul = 1.0f;
	f32 pmul = 1.0f;
	f32 div = 0.0f;
	for (u32 i = 0; i < octaves; ++i) {
		result += fn(seed * pmul) * vmul;
		div += vmul;
		vmul *= persistence;
		pmul *= 2.0f;
	}
	return result / div;
}
struct Hit {
	V3 p, n;
};
bool raycastPlane(V3 a, V3 b, V3 s1, V3 s2, V3 s3, Hit& hit) {
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
}
bool raycastBlock(V3 a, V3 b, V3 blk, Hit& hit, V3 blockDimensions) {
	Hit hits[6];
	bool results[6] {};
	f32 x = blockDimensions.x;
	f32 y = blockDimensions.y;
	f32 z = blockDimensions.z;
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
	hit = hits[min];
	return true;
}
template<class T, size_t size>
T linearSample(const T (&arr)[size], float t) {
	f32 f = frac(t) * size;
	int a = (int)f;
	int b = a + 1;
	if (b == size) 
		b = 0;
	return lerp(arr[a], arr[b], frac(f));
}