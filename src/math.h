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

template<class T> ce T min(T a, T b) { return a < b ? a : b; }
template<class T> ce T max(T a, T b) { return a > b ? a : b; }

struct V2i;
struct V3i;
struct V4i;

struct V2 {
	f32 x = 0;
	f32 y = 0;
	ce V2() = default;
	ce V2(f32 x, f32 y) : x(x), y(y) {}
	ce V2(f32 v) : V2(v, v) {}
	template<class T>
	ce explicit V2(T x, T y) : V2((f32)x, (f32)y) {}
	ce explicit V2(V2i b);
	ce V2 operator-() const { return {-x,-y}; }
	ce V2 operator+(V2 b) const { return {x + b.x,y + b.y}; }
	ce V2 operator-(V2 b) const { return {x - b.x,y - b.y}; }
	ce V2 operator*(V2 b) const { return {x * b.x,y * b.y}; }
	ce V2 operator/(V2 b) const { return {x / b.x,y / b.y}; }
	ce V2& operator+=(V2 b) { *this = *this + b; return *this; }
	ce V2& operator-=(V2 b) { *this = *this - b; return *this; }
	ce V2& operator*=(V2 b) { *this = *this * b; return *this; }
	ce V2& operator/=(V2 b) { *this = *this / b; return *this; }
	ce bool operator==(V2 b) const { return x == b.x && y == b.y; }
	ce bool operator!=(V2 b) const { return !(*this == b); }
	ce f32 dot(V2 b) const {
		return x * b.x + y * b.y;
	}
	ce f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	V2 normalized() const {
		return *this / length();
	}
	ce f32* data() { return &x; }
	ce const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream& os, V2 v) {
		return os << v.x << ", " << v.y;
	}
};
struct V3 {
	f32 x = 0;
	f32 y = 0;
	f32 z = 0;
	ce V3() = default;
	ce V3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
	ce V3(f32 v) : V3(v, v, v) {}
	ce explicit V3(V3i b);
	ce V3 operator-() const { return {-x,-y,-z}; }
	ce V3 operator+(V3 b) const { return {x + b.x,y + b.y,z + b.z}; }
	ce V3 operator-(V3 b) const { return {x - b.x,y - b.y,z - b.z}; }
	ce V3 operator*(V3 b) const { return {x * b.x,y * b.y,z * b.z}; }
	ce V3 operator/(V3 b) const { return {x / b.x,y / b.y,z / b.z}; }
	ce V3& operator+=(V3 b) { *this = *this + b; return *this; }
	ce V3& operator-=(V3 b) { *this = *this - b; return *this; }
	ce V3& operator*=(V3 b) { *this = *this * b; return *this; }
	ce V3& operator/=(V3 b) { *this = *this / b; return *this; }
	ce bool operator==(V3 b) const { return x == b.x && y == b.y && z == b.z; }
	ce bool operator!=(V3 b) const { return !(*this == b); }
	ce f32 dot(V3 b) const {
		return x * b.x + y * b.y + z * b.z;
	}
	ce f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	ce V2 xz() const {
		return {x, z};
	}
	ce V3 cross(V3 b) const {
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
	ce f32* data() { return &x; }
	ce const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream& os, V3 v) {
		return os << v.x << ", " << v.y << ", " << v.z;
	}
};
struct alignas(16) V4 {
	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
			f32 w;
		};
		f32 v[4];
		__m128 m;
	};
	ce V4() : v() {}
	ce V4(__m128 m) : m(m) {}
	ce V4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
	ce V4(f32 v) : V4(v, v, v, v) {}
	ce explicit V4(V4i b);
	ce V4 operator+(V4 b) const { return _mm_add_ps(m, b.m); }
	ce V4 operator-(V4 b) const { return _mm_sub_ps(m, b.m); }
	ce V4 operator*(V4 b) const { return _mm_mul_ps(m, b.m); }
	ce V4 operator/(V4 b) const { return _mm_div_ps(m, b.m); }
	ce V4& operator+=(V4 b) { *this = *this + b; return *this; }
	ce V4& operator-=(V4 b) { *this = *this - b; return *this; }
	ce V4& operator*=(V4 b) { *this = *this * b; return *this; }
	ce V4& operator/=(V4 b) { *this = *this / b; return *this; }
	ce bool operator==(V4 b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
	ce bool operator!=(V4 b) const { return !(*this == b); }
	ce V4 operator-() {
		return _mm_sub_ps(_mm_set_ps1(0), m);
	}
	ce f32* data() { return &x; }
	ce const f32* data() const { return &x; }
	friend std::ostream& operator<<(std::ostream & os, V4 v) {
		return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
	}
	f32 dot(V4 b) const {
		f32 result;
		_mm_store_ss(&result, _mm_dp_ps(m, b.m, 0xFF));
		return result;
	}
	f32 lengthSqr() const {
		return dot(*this);
	}
	f32 length() const {
		return sqrtf(lengthSqr());
	}
	ce V3 xyz() const ne {
		return {x,y,z};
	}
};
struct V2i {
	i32 x = 0;
	i32 y = 0;
	ce V2i() = default;
	ce V2i(i32 x, i32 y) : x(x), y(y) {}
	ce V2i(i32 v) : V2i(v, v) {}
	ce V2i operator-() const { return {-x,-y}; }
	ce V2i operator+(V2i b) const { return {x + b.x,y + b.y}; }
	ce V2i operator-(V2i b) const { return {x - b.x,y - b.y}; }
	ce V2i operator*(V2i b) const { return {x * b.x,y * b.y}; }
	ce V2i operator/(V2i b) const { return {x / b.x,y / b.y}; }
	ce V2i operator%(V2i b) const { return {x % b.x,y % b.y}; }
	ce friend V2i operator+(i32 a, V2i b) { return {a + b.x,a + b.y}; }
	ce friend V2i operator-(i32 a, V2i b) { return {a - b.x,a - b.y}; }
	ce friend V2i operator*(i32 a, V2i b) { return {a * b.x,a * b.y}; }
	ce friend V2i operator/(i32 a, V2i b) { return {a / b.x,a / b.y}; }
	ce friend V2i operator%(i32 a, V2i b) { return {a % b.x,a % b.y}; }
	ce V2i& operator+=(V2i b) { *this = *this + b; return *this; }
	ce V2i& operator-=(V2i b) { *this = *this - b; return *this; }
	ce V2i& operator*=(V2i b) { *this = *this * b; return *this; }
	ce V2i& operator/=(V2i b) { *this = *this / b; return *this; }
	ce V2i& operator%=(V2i b) { *this = *this % b; return *this; }
	ce bool operator==(V2i b) const { return x == b.x && y == b.y; }
	ce bool operator!=(V2i b) const { return !(*this == b); }
	friend std::ostream& operator<<(std::ostream& os, V2i v) {
		return os << v.x << ", " << v.y;
	}
};
struct V3i {
	i32 x = 0;
	i32 y = 0;
	i32 z = 0;
	ce V3i() = default;
	ce V3i(i32 x, i32 y, i32 z) : x(x), y(y), z(z) {}
	ce V3i(i32 v) : V3i(v, v, v) {}
	ce explicit V3i(V3 b) : V3i((i32)b.x, (i32)b.y, (i32)b.z) {}
	ce V3i operator-() const { return {-x,-y,-z}; }
	ce V3i operator+(V3i b) const { return {x + b.x,y + b.y,z + b.z}; }
	ce V3i operator-(V3i b) const { return {x - b.x,y - b.y,z - b.z}; }
	ce V3i operator*(V3i b) const { return {x * b.x,y * b.y,z * b.z}; }
	ce V3i operator/(V3i b) const { return {x / b.x,y / b.y,z / b.z}; }
	ce V3i operator%(V3i b) const { return {x % b.x,y % b.y,z % b.z}; }
	ce V3i& operator+=(V3i b) { *this = *this + b; return *this; }
	ce V3i& operator-=(V3i b) { *this = *this - b; return *this; }
	ce V3i& operator*=(V3i b) { *this = *this * b; return *this; }
	ce V3i& operator/=(V3i b) { *this = *this / b; return *this; }
	ce V3i& operator%=(V3i b) { *this = *this % b; return *this; }
	ce bool operator==(V3i b) const { return x == b.x && y == b.y && z == b.z; }
	ce bool operator!=(V3i b) const { return !(*this == b); }
	ce V3i absolute() const {
		return {
			abs(x),
			abs(y),
			abs(z)
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
	ce V4i() = default;
	ce V4i(i32 x, i32 y, i32 z, i32 w) : x(x), y(y), z(z), w(w) {}
	ce V4i(i32 v) : V4i(v, v, v, v) {}
	ce explicit V4i(V4 b);
	ce V4i operator-() const { return {-x,-y,-z,-w}; }
	ce V4i operator+(V4i b) const { return {x + b.x,y + b.y,z + b.z,w + b.w}; }
	ce V4i operator-(V4i b) const { return {x - b.x,y - b.y,z - b.z,w - b.w}; }
	ce V4i operator*(V4i b) const { return {x * b.x,y * b.y,z * b.z,w * b.w}; }
	ce V4i operator/(V4i b) const { return {x / b.x,y / b.y,z / b.z,w / b.w}; }
	ce V4i operator%(V4i b) const { return {x % b.x,y % b.y,z % b.z,w % b.w}; }
	ce V4i& operator+=(V4i b) { *this = *this + b; return *this; }
	ce V4i& operator-=(V4i b) { *this = *this - b; return *this; }
	ce V4i& operator*=(V4i b) { *this = *this * b; return *this; }
	ce V4i& operator/=(V4i b) { *this = *this / b; return *this; }
	ce V4i& operator%=(V4i b) { *this = *this % b; return *this; }
	ce bool operator==(V4i b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
	ce bool operator!=(V4i b) const { return !(*this == b); }
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
		f32 v[16];
		__m128 m[4];
	};
	ce M4() : v() {}
	ce M4(V4 i, V4 j, V4 k, V4 l) : i(i), j(j), k(k), l(l) {}
	ce M4(f32 ix, f32 iy, f32 iz, f32 iw,
	      f32 jx, f32 jy, f32 jz, f32 jw,
	      f32 kx, f32 ky, f32 kz, f32 kw,
	      f32 lx, f32 ly, f32 lz, f32 lw) :
		i(ix, iy, iz, iw),
		j(jx, jy, jz, jw),
		k(kx, ky, kz, kw),
		l(lx, ly, lz, lw) {
	}
	V4 operator*(V4 b) const {
		__m128 x = _mm_mul_ps(_mm_set_ps1(b.x), m[0]);
		__m128 y = _mm_mul_ps(_mm_set_ps1(b.y), m[1]);
		__m128 z = _mm_mul_ps(_mm_set_ps1(b.z), m[2]);
		__m128 w = _mm_mul_ps(_mm_set_ps1(b.w), m[3]);
		return _mm_add_ps(_mm_add_ps(x, y), _mm_add_ps(z, w));
	}
	V3 operator*(V3 b) const {
		__m128 x = _mm_mul_ps(_mm_set_ps1(b.x), m[0]);
		__m128 y = _mm_mul_ps(_mm_set_ps1(b.y), m[1]);
		__m128 z = _mm_mul_ps(_mm_set_ps1(b.z), m[2]);
		__m128 r = _mm_add_ps(x, _mm_add_ps(y, z));
		V3 result;
		memcpy(&result, &r, 12);
		return result;
	}
	M4 operator*(M4 b) const {
		return {
			*this * b.i,
			*this * b.j,
			*this * b.k,
			*this * b.l
		};
	}
	M4& operator*=(M4 b) { return *this = *this * b; }
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
	static M4 rotationZXY(V3 v) {
		return (rotationY(v.y) * rotationX(v.x)) * rotationZ(v.z);
	}
	//Yaw, Pitch, Roll (YXZ)
	static M4 rotationYXZ(V3 v) {
		return (rotationZ(v.z) * rotationX(v.x)) * rotationY(v.y);
	}
	friend std::ostream& operator<<(std::ostream & os, M4 m) {
		return os << m.i << ", " << m.j << ", " << m.k << ", " << m.l;
	}
};
ce u8 randomU8(u8 r) {
	r += 0x0C;
	r *= 0x61;
	r ^= 0xB2;
	r -= 0x80;
	r ^= 0xF5;
	r *= 0xA7;
	return (r << 4) | (r >> 4);
}
ce u32 randomU32(u32 r) {
	r += 0x0C252DA0;
	r *= 0x55555561;
	r ^= 0xB23E2387;
	r -= 0x8069BAC0;
	r ^= 0xF5605798;
	r *= 0xAAAAAABF;
	return (r << 16) | (r >> 16);
}
ce u32 randomU32(i32 in) {
	return randomU32((u32)in);
}
ce u32 randomU32(V2i in) {
	auto x = randomU32(in.x);
	auto y = randomU32(in.y);
	return x ^ y;
}
ce u32 randomU32(V3i in) {
	auto x = randomU32(in.x);
	auto y = randomU32(in.y);
	auto z = randomU32(in.z);
	return x + y + z;
}
ce u64 randomU64(V3i in) {
	auto x = randomU32(in.x);
	auto y = randomU32(in.y);
	auto z = randomU32(in.z);
	return
		(u64)x | ((u64)y << 32) +
		(u64)z | ((u64)x << 32) +
		(u64)y | ((u64)z << 32);
}
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
#define round _round
ce i32 round(f32 v) {
	if (v < 0)
		return int(v - .5f);
	return int(v + .5f);
}
ce f32 frac(f32 x) {
	auto r = x - (i64)x;
	if (r < 0)
		++r;
	return r;
}
ce V2 frac(V2 v) {
	return {
		frac(v.x),
		frac(v.y),
	};
}
ce V3 frac(V3 v) {
	return {
		frac(v.x),
		frac(v.y),
		frac(v.z),
	};
}
ce V4 frac(V4 v) {
	return {
		frac(v.x),
		frac(v.y),
		frac(v.z),
		frac(v.w),
	};
}
ce i32 frac(i32 v, i32 s) {
	if (v < 0)
		return (v + 1) % s + s - 1;
	return v % s;
}
ce V2i frac(V2i v, i32 s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
	};
}
ce V3i frac(V3i v, i32 s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
		frac(v.z, s),
	};
}
ce V4i frac(V4i v, i32 s) {
	return {
		frac(v.x, s),
		frac(v.y, s),
		frac(v.z, s),
		frac(v.w, s),
	};
}
ce V2 floor(V2 v) {
	return {
		floorf(v.x),
		floorf(v.y),
	};
}
ce V3 floor(V3 v) {
	return {
		floorf(v.x),
		floorf(v.y),
		floorf(v.z),
	};
}
ce V4 floor(V4 v) {
	return {
		floorf(v.x),
		floorf(v.y),
		floorf(v.z),
		floorf(v.w),
	};
}
ce i32 floor(i32 v, i32 s) {
	if (v < 0)
		return (v + 1) / s - 1;
	return v / s;
}
ce V2i floor(V2i v, i32 step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
	};
}
ce V3i floor(V3i v, i32 step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
		floor(v.z, step),
	};
}
ce V4i floor(V4i v, i32 step) {
	return {
		floor(v.x, step),
		floor(v.y, step),
		floor(v.z, step),
		floor(v.w, step),
	};
}
ce f32 dot(V2 a, V2 b) { return a.dot(b); }
ce f32 dot(V3 a, V3 b) { return a.dot(b); }
f32 dot(V4 a, V4 b) { return a.dot(b); }
ce f32 distanceSqr(V2 a, V2 b) { return (a - b).lengthSqr(); }
ce f32 distanceSqr(V3 a, V3 b) { return (a - b).lengthSqr(); }
f32 distanceSqr(V4 a, V4 b) { return (a - b).lengthSqr(); }
f32 distance(V2 a, V2 b) { return sqrt(distanceSqr(a, b)); }
f32 distance(V3 a, V3 b) { return sqrt(distanceSqr(a, b)); }
f32 distance(V4 a, V4 b) { return sqrt(distanceSqr(a, b)); }
int maxDistance(V3i a, V3i b) {
	a = (a - b).absolute();
	return max(max(a.x, a.y), a.z);
}
template<class T>
ce T clamp(T a, T mi, T ma) {
	return min(max(a, mi), ma);
}
template<class T>
ce T lerp(T a, T b, f32 t) {
	return a + (b - a) * t;
}
f32 cos01(f32 t) {
	return 0.5f - cosf(t * PI) * 0.5f;
}
ce f32 noise(i32 x) {
	x = (x << 13) ^ x;
	return (1.0f - ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}
ce f32 noise(V2i v) {
	auto n = v.x + v.y * 57;
	n = (n << 13) ^ n;
	return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 2147483648.0f);
}
f32 cosNoise(f32 v) {
	auto fl = (i32)floor(v);
	return lerp(noise(fl),
				noise(fl + 1),
				cos01(frac(v)));
}
template<class Interp, class Sample>
f32 interpolate(V2 p, Interp&& interp, Sample&& sample) {
	auto x = (i32)floor(p.x);
	auto y = (i32)floor(p.y);
	auto fr = frac(p.x);
	return interp(interp(sample(x, y),     sample(x + 1, y),     fr),
				  interp(sample(x, y + 1), sample(x + 1, y + 1), fr),
				  frac(p.y));
}
template<class Interp, class Sample>
f32 interpolate(V2i p, i32 s, Interp&& interp, Sample&& sample) {
	auto fl = floor(p, s);
	auto fr = frac(p.x, s) / f32(s);
	return interp(interp(sample(fl+V2i{0,0}), sample(fl+V2i{1,0}), fr),
				  interp(sample(fl+V2i{0,1}), sample(fl+V2i{1,1}), fr),
				  frac(p.y, s) / f32(s));
}
ce V2 random01(V2 p) {
	V2 a = frac((p + ROOT2) * V2 { PI, PI * 2 });
	a += dot(a, a + PI * 4);
	return frac(V2 {a.x * a.y, a.x + a.y});
}
ce V3 random01(V3 p) {
	V3 a = frac((p + ROOT2) * V3 { PI, PI * 2, PI * 3 });
	a += dot(a, a + PI * 4);
	return frac(V3 {a.x * a.y, a.y * a.z, a.x * a.z});
}
ce V2 random01(V2i v) {
	auto x = randomU32(v);
	auto y = randomU32(v + 2454940283);
	return V2 {x / 256, y/256} / 16777215.f;
}
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
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {0,-1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {0, 0})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {0, 1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {1,-1})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {1, 0})));
	minDist = min(minDist, distanceSqr(rel, getPos(V2 {1, 1})));
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
f32 voronoi(V2i v, i32 cellSize) {
	V2 rel = (V2 { frac(v, cellSize) } / (f32)cellSize) - 0.5f;
	V2i tile = floor(v, cellSize);
	f32 minDist = 1000;
	auto getPos = [tile](V2i off) { return random01(tile + off) + V2{off} - 0.5f; };
	for (i32 x = -1; x <= 1; ++x) {
		minDist = min(minDist, distanceSqr(rel, getPos({x,-1})));
		minDist = min(minDist, distanceSqr(rel, getPos({x, 0})));
		minDist = min(minDist, distanceSqr(rel, getPos({x, 1})));
	}
	return sqrt(minDist) * (1 / ROOT2);
}
f32 voronoiCrackle(V2 v) {
	V2 rel = frac(v) - 0.5f;
	V2 tile = floor(v);
	f32 closestPoi32s[3] {8,8,8};
	for (i32 x = -2; x <= 2; ++x) {
		for (i32 y = -2; y <= 2; ++y) {
			V2 off {(f32)x, (f32)y};
			auto dist = distanceSqr(rel, off + random01(tile + off) - 0.5f);
			float maxClosestDist = 0.;
			i32 maxClosestDistIdx = 0;
			for (i32 i=0; i < _countof(closestPoi32s); ++i) {
				if (closestPoi32s[i] > maxClosestDist) {
					maxClosestDist = closestPoi32s[i];
					maxClosestDistIdx = i;
				}
			}
			if (dist < maxClosestDist) {
				closestPoi32s[maxClosestDistIdx] = dist;
			}
		}
	}
	return sqrt(max(max(closestPoi32s[0], closestPoi32s[1]), closestPoi32s[2])) * (1 / ROOT2);
}
template<class Fn>
auto textureDetail(u32 octaves, f32 persistence, V2 seed, Fn&& fn) {
	decltype(fn(seed)) result {};
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
template<class Fn>
auto textureDetail(u32 octaves, f32 persistence, V2i seed, i32 s, Fn&& fn) {
	decltype(fn(seed, s)) result {};
	f32 vmul = 1.0f;
	i32 pmul = 1;
	f32 div = 0.0f;
	for (u32 i = 0; i < octaves; ++i) {
		result += fn(seed * pmul + i * s, s) * vmul;
		div += vmul;
		vmul *= persistence;
		pmul *= 2;
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
	i32 min = -1;
	f32 minDist = FLT_MAX;
	for (i32 i = 0; i < 6; ++i) {
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
ce T linearSample(const T (&arr)[size], float t) ne {
	f32 f = frac(t) * size;
	i32 a = (i32)f;
	i32 b = a + 1;
	if (b == size)
		b = 0;
	return lerp(arr[a], arr[b], frac(f));
}
struct FrustumPlanes {
	V4 planes[6];
	ce FrustumPlanes(const M4& vp) ne {
		planes[0].x = vp.i.w + vp.i.x;
		planes[0].y = vp.j.w + vp.j.x;
		planes[0].z = vp.k.w + vp.k.x;
		planes[0].w = vp.l.w + vp.l.x;
		planes[1].x = vp.i.w - vp.i.x;
		planes[1].y = vp.j.w - vp.j.x;
		planes[1].z = vp.k.w - vp.k.x;
		planes[1].w = vp.l.w - vp.l.x;
		planes[2].x = vp.i.w - vp.i.y;
		planes[2].y = vp.j.w - vp.j.y;
		planes[2].z = vp.k.w - vp.k.y;
		planes[2].w = vp.l.w - vp.l.y;
		planes[3].x = vp.i.w + vp.i.y;
		planes[3].y = vp.j.w + vp.j.y;
		planes[3].z = vp.k.w + vp.k.y;
		planes[3].w = vp.l.w + vp.l.y;
		planes[5].x = vp.i.w - vp.i.z;
		planes[5].y = vp.j.w - vp.j.z;
		planes[5].z = vp.k.w - vp.k.z;
		planes[5].w = vp.l.w - vp.l.z;
		planes[4].x = vp.i.z;
		planes[4].y = vp.j.z;
		planes[4].z = vp.k.z;
		planes[4].w = vp.l.z;
		for (auto& p : planes) {
			auto length = V3 {p.x, p.y, p.z}.length();
			p /= length;
		}
	}
	nd ce bool containsSphere(V3 position, float radius) const ne {
		for (auto& p : planes) {
			if (V3 {p.x, p.y, p.z}.dot(position) + p.w + radius < 0) {
				return false;
			}
		}
		return true;
	}
};

#if 1
namespace {
constexpr f32 test[4] {
		0,1,2,3
};
static_assert(linearSample(test, 0) == 0);
static_assert(linearSample(test, 0.125f) == 0.5f);
static_assert(linearSample(test, 0.25f) == 1);
static_assert(linearSample(test, 0.375f) == 1.5f);
static_assert(linearSample(test, 0.5f) == 2);
static_assert(linearSample(test, 0.625f) == 2.5);
static_assert(linearSample(test, 0.75f) == 3);
static_assert(linearSample(test, 0.875f) == 1.5f);
static_assert(linearSample(test, 1) == 0);
static_assert(frac(-6, 5) == 4);
static_assert(frac(-5, 5) == 0);
static_assert(frac(-4, 5) == 1);
static_assert(frac(-3, 5) == 2);
static_assert(frac(-2, 5) == 3);
static_assert(frac(-1, 5) == 4);
static_assert(frac(0, 5) == 0);
static_assert(frac(1, 5) == 1);
static_assert(frac(2, 5) == 2);
static_assert(frac(3, 5) == 3);
static_assert(frac(4, 5) == 4);
static_assert(frac(5, 5) == 0);
static_assert(frac(6, 5) == 1);
static_assert(floor(-6, 3) == -2);
static_assert(floor(-5, 3) == -2);
static_assert(floor(-4, 3) == -2);
static_assert(floor(-3, 3) == -1);
static_assert(floor(-2, 3) == -1);
static_assert(floor(-1, 3) == -1);
static_assert(floor(0, 3) == 0);
static_assert(floor(1, 3) == 0);
static_assert(floor(2, 3) == 0);
static_assert(floor(3, 3) == 1);
static_assert(floor(4, 3) == 1);
static_assert(floor(5, 3) == 1);
static_assert(floor(6, 3) == 2);
static_assert(round(-1) == -1);
static_assert(round(-0.6) == -1);
static_assert(round(-0.5) == -1);
static_assert(round(-0.4) == 0);
static_assert(round(0) == 0);
static_assert(round(0.4) == 0);
static_assert(round(0.5) == 1);
static_assert(round(0.6) == 1);
static_assert(round(1) == 1);
}
#endif
