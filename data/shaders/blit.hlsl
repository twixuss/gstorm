#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float2 uv : UV;
};
Texture2D blitTex : register(t0);
SamplerState samplerState : register(s0);
void vMain(in uint id : SV_VertexID, out V2P o) {
	const float2 positions[] = {
		float2(-1,-1),
		float2(-1,3),
		float2(3,-1),
	};
	const float2 uvs[] = {
		float2(0, 1),
		float2(0, -1),
		float2(2, 1),
	};
	o.position = float4(positions[id], 0.5f, 1);
	o.uv = uvs[id];
}
float4 blitSample(float2 uv) {
	return blitTex.SampleLevel(samplerState, uv, 0);
}
#if BLIT_SS == 1
void pMain(in V2P i, out float4 oColor : SV_Target) {
	oColor = blitSample(i.uv);
}
#elif BLIT_SS == 2
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 off = float4(sampleOffset, -sampleOffset);
	float4 col = 0;
	col += blitSample(i.uv + off.xy);
	col += blitSample(i.uv + off.xw);
	col += blitSample(i.uv + off.zy);
	col += blitSample(i.uv + off.zw);
	oColor = col * 0.25;
}
#elif BLIT_SS == 3
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 off = float4(sampleOffset, -sampleOffset);
	float4 col = 0;
	col += blitSample(i.uv);
	col += blitSample(i.uv + off.xy);
	col += blitSample(i.uv + off.xw);
	col += blitSample(i.uv + off.zy);
	col += blitSample(i.uv + off.zw);
	col += blitSample(i.uv + float2(off.x, 0));
	col += blitSample(i.uv + float2(off.z, 0));
	col += blitSample(i.uv + float2(0, off.y));
	col += blitSample(i.uv + float2(0, off.w));
	oColor = col * (1.0 / 9.0);
}
#elif BLIT_SS == 4
float4 samplePart(float2 uv, float2 off) {
	float4 o = float4(off, -off);
	return blitSample(uv + o.xy) +
		blitSample(uv + o.xw) +
		blitSample(uv + o.zy) +
		blitSample(uv + o.zw);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	float4 col = 0;
	col += samplePart(i.uv, sampleOffset);
	col += samplePart(i.uv, sampleOffset * 3);
	col += samplePart(i.uv, sampleOffset * float2(1, 3));
	col += samplePart(i.uv, sampleOffset * float2(3, 1));
	oColor = col * 0.0625;
}
#else
#error Bad super sample level
#endif
