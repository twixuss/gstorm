#include "math.h"

V2::V2(V2i b) : V2((f32)b.x, (f32)b.y) {}
V3::V3(V3i b) : V3((f32)b.x, (f32)b.y, (f32)b.z) {}
V4::V4(V4i b) : V4((f32)b.x, (f32)b.y, (f32)b.z, (f32)b.w) {}
V3i V3::rounded() const {
	return V3i(lroundf(x), lroundf(y), lroundf(z));
}
V3i::V3i(V3 b) : V3i((i32)b.x, (i32)b.y, (i32)b.z) {}
