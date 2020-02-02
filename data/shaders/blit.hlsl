#include "common.h"
struct V2P {
	float4 position : SV_Position;
	float2 uv : UV;
};
Texture2D blitTex : register(t0);
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
float4 blitSampleP(float2 uv) {
	return blitTex.SampleLevel(pointSamplerState, uv, 0);
}
float4 blitSampleL(float2 uv) {
	return blitTex.SampleLevel(linearSamplerState, uv, 0);
}
float4 samplePart(float2 uv, float2 off) {
	float4 o = float4(off, -off);
	return 
		blitSampleP(uv + o.xy) +
		blitSampleP(uv + o.xw) +
		blitSampleP(uv + o.zy) +
		blitSampleP(uv + o.zw);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
#if BLIT_SS == 1
	oColor = blitSampleP(i.uv);
#elif BLIT_SS == 2
	oColor = blitSampleL(i.uv);
#elif BLIT_SS == 3
	float4 off = float4(sampleOffset, -sampleOffset);
	float4 col = 0;
	col += blitSampleP(i.uv);
	col += blitSampleP(i.uv + off.xy);
	col += blitSampleP(i.uv + off.xw);
	col += blitSampleP(i.uv + off.zy);
	col += blitSampleP(i.uv + off.zw);
	col += blitSampleP(i.uv + float2(off.x, 0));
	col += blitSampleP(i.uv + float2(off.z, 0));
	col += blitSampleP(i.uv + float2(0, off.y));
	col += blitSampleP(i.uv + float2(0, off.w));
	oColor = col * (1.0 / 9.0);
#elif BLIT_SS == 4
#if 1
	float4 off = float4(sampleOffset, -sampleOffset);
	float4 col = 0;
	col += blitSampleL(i.uv + off.xy);
	col += blitSampleL(i.uv + off.xw);
	col += blitSampleL(i.uv + off.zy);
	col += blitSampleL(i.uv + off.zw);
	oColor = col * 0.25;
#else
	float4 col = 0;
	col += samplePart(i.uv, sampleOffset);
	col += samplePart(i.uv, sampleOffset * 3);
	col += samplePart(i.uv, sampleOffset * float2(1, 3));
	col += samplePart(i.uv, sampleOffset * float2(3, 1));
	oColor = col * 0.0625;
#endif
#else
#error Bad super sample level
#endif
}
