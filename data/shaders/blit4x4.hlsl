cbuffer _ : register(b3) {
	float2 sampleOffset;
}
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
float4 samplePart(float2 uv, float2 off) {
	float4 o = float4(off, -off);
	return blitTex.SampleLevel(samplerState, uv + o.xy, 0) +
		   blitTex.SampleLevel(samplerState, uv + o.xw, 0) +
		   blitTex.SampleLevel(samplerState, uv + o.zy, 0) +
		   blitTex.SampleLevel(samplerState, uv + o.zw, 0);
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
#if 1
	float4 col = 0;
	col += samplePart(i.uv, sampleOffset);
	col += samplePart(i.uv, sampleOffset * 3);
	col += samplePart(i.uv, sampleOffset * float2(1, 3));
	col += samplePart(i.uv, sampleOffset * float2(3, 1));
	oColor = col * 0.0625;
#else
	oColor = blitTex.SampleLevel(samplerState, i.uv, 0);
#endif
}