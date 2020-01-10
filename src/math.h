#pragma once
#include "common.h"
#define PI 3.141592653f
#define PI2 (PI * 2.0f)
#define INVPI (1.0f / PI)
#define INVPI2 (1.0f / PI2)
#define DEG2RAD(deg) ((deg) / 360.0f * PI2)
#define RAD2DEG(rad) ((rad) / PI2 * 360.0f)

struct V2i;
struct V3i;

struct V2 {
	f32 x = 0;
	f32 y = 0;
	V2() = default;
	V2(f32 x, f32 y) : x(x), y(y) {}
	V2(f32 v) : V2(v, v) {}
	V2 operator-() const { return {-x,-y}; }
	V2 operator+(V2 b) const { return {x + b.x,y + b.y}; }
	V2 operator-(V2 b) const { return {x - b.x,y - b.y}; }
	V2 operator*(V2 b) const { return {x * b.x,y * b.y}; }
	V2 operator/(V2 b) const { return {x / b.x,y / b.y}; }
	V2& operator+=(V2 b) { *this = *this + b; return *this; }
	V2& operator-=(V2 b) { *this = *this - b; return *this; }
	V2& operator*=(V2 b) { *this = *this * b; return *this; }
	V2& operator/=(V2 b) { *this = *this / b; return *this; }
	bool operator==(V2 b) const { return x == b.x && y == b.y; }
	bool operator!=(V2 b) const { return !(*this == b); }
	f32 dot(V2 b) const {
		return x * b.x + y * b.y;
	}
	f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
};
struct V3 {
	f32 x = 0;
	f32 y = 0;
	f32 z = 0;
	V3() = default;
	V3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
	V3(f32 v) : V3(v, v, v) {}
	explicit V3(V3i b);
	V3 operator-() const { return {-x,-y,-z}; }
	V3 operator+(V3 b) const { return {x + b.x,y + b.y,z + b.z}; }
	V3 operator-(V3 b) const { return {x - b.x,y - b.y,z - b.z}; }
	V3 operator*(V3 b) const { return {x * b.x,y * b.y,z * b.z}; }
	V3 operator/(V3 b) const { return {x / b.x,y / b.y,z / b.z}; }
	V3& operator+=(V3 b) { *this = *this + b; return *this; }
	V3& operator-=(V3 b) { *this = *this - b; return *this; }
	V3& operator*=(V3 b) { *this = *this * b; return *this; }
	V3& operator/=(V3 b) { *this = *this / b; return *this; }
	bool operator==(V3 b) const { return x == b.x && y == b.y && z == b.z; }
	bool operator!=(V3 b) const { return !(*this == b); }
	f32 dot(V3 b) const {
		return x * b.x + y * b.y + z * b.z;
	}
	f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	V2 xz() const {
		return {x, z};
	}
	V3 cross(V3 b) const {
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
};
struct alignas(16) V4 {
	f32 x = 0;
	f32 y = 0;
	f32 z = 0;
	f32 w = 0;
	V4() = default;
	V4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
	V4(f32 v) : V4(v, v, v, v) {}
	V4 operator-() {
		return {-x,-y,-z,-w};
	}
};
struct V2i {
	i32 x = 0;
	i32 y = 0;
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
	V4 operator*(V4 b) {
		return {
			i.x * b.x + j.x * b.y + k.x * b.z + l.x * b.w,
			i.y * b.x + j.y * b.y + k.y * b.z + l.y * b.w,
			i.z * b.x + j.z * b.y + k.z * b.z + l.z * b.w,
			i.w * b.x + j.w * b.y + k.w * b.z + l.w * b.w,
		};
	}
	V3 operator*(V3 b) {
		return {
			i.x * b.x + j.x * b.y + k.x * b.z,
			i.y * b.x + j.y * b.y + k.y * b.z,
			i.z * b.x + j.z * b.y + k.z * b.z,
		};
	}
	M4 operator*(M4 b) {
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
};
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
inline f32 frac(f32 x) {
	return x - floorf(x);
}
inline V3 frac(V3 v) {
	return {
		frac(v.x),
		frac(v.y),
		frac(v.z)
	};
}
inline V3 floor(V3 v) {
	return {
		floorf(v.x),
		floorf(v.y),
		floorf(v.z)
	};
}
f32 dot(V3 a, V3 b) {
	return a.dot(b);
}
f32 distanceSqr(V3 a, V3 b) {
	return (a - b).lengthSqr();
}
f32 distance(V3 a, V3 b) {
	return sqrt(distanceSqr(a, b));
}
template<class T>
T min(T a, T b) {
	return a < b ? a : b;
}
template<class T>
T max(T a, T b) {
	return a > b ? a : b;
}
f32 lerp(f32 a, f32 b, f32 t) {
	return a + (b - a) * t;
}
f32 coserp(f32 a, f32 b, f32 t) {
	auto ft = t * PI;
	auto f = (1 - cosf(ft)) * 0.5f;
	return a * (1 - f) + b * f;
}
f32 noise(i32 x, i32 y) {
	if (x < 0) x = -x + 2;
	if (y < 0) y = -y + 2;
	auto n = x + y * 57;
	n = (n << 13) ^ n;
	return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 2147483648.0f);
}
f32 smoothNoise(i32 x, i32 y) {
	auto corners = (noise(x - 1, y - 1) + noise(x + 1, y - 1) + noise(x - 1, y + 1) + noise(x + 1, y + 1)) / 16;
	auto sides   = (noise(x - 1, y) + noise(x + 1, y) + noise(x, y - 1) + noise(x, y + 1)) / 8;
	auto center  =  noise(x, y) / 4;
	return corners + sides + center;
}
f32 cosNoise(f32 x, f32 y) {
	auto bl = noise((int)x, (int)y);
	auto tl = noise((int)x, (int)y + 1);
	auto br = noise((int)x + 1, (int)y);
	auto tr = noise((int)x + 1, (int)y + 1);
	auto b = coserp(bl, br, frac(x));
	auto t = coserp(tl, tr, frac(x));
	return coserp(b, t, frac(y));
}
f32 perlin(f32 x, f32 y, u32 o) {
	f32 result = 0.0f;
	f32 vmul = 1.0f;
	f32 pmul = 1.0f;
	f32 div = 0.0f;
	for (u32 i = 0; i < o; ++i) {
		result += cosNoise(x * pmul, y * pmul) * vmul;
		div += vmul;
		vmul *= 0.5f;
		pmul *= 2.0f;
	}
	return result / div;
};
V3 random01(V3 p) {
	V3 a = frac(p * V3 {PI, PI * 2, PI * 3});
	a += dot(a, a + PI * 4);
	return frac(V3 {a.x * a.y, a.y * a.z, a.x * a.z});
};
f32 voronoi(V3 v) {
	V3 rel = frac(v) - 0.5f;
	V3 tile = floor(v);
	f32 minDist = 1000;
	for (i32 z = -1; z <= 1; ++z) {
		for (i32 x = -1; x <= 1; ++x) {
			for (i32 y = -1; y <= 1; ++y) {
				V3 off {(f32)x, (f32)y, (f32)z};
				minDist = min(minDist, distanceSqr(rel, random01(tile + off) + off - 0.5f));
			}
		}
	}
	return sqrt(minDist);
}
template<class T>
struct ToInt {
	using Type = std::conditional_t<
		std::is_enum_v<T>,
		std::underlying_type_t<T>,
			std::conditional_t<
			std::is_integral_v<T>,
			T,
			u32>>;
};

template<size_t numBits>
struct Bitfield {
	u32 field : numBits;
	constexpr Bitfield() {
		field = 0;
	}
	template<class Int>
	constexpr void set(Int idx, bool val) {
		if (val) 
			field |= 1 << (ToInt<Int>::Type)idx;
		else 
			field &= ~(1 << (ToInt<Int>::Type)idx);
	}
	template<class Int>
	constexpr bool get(Int idx) const {
		return (bool)(field & 1 << (ToInt<Int>::Type)idx);
	}
};