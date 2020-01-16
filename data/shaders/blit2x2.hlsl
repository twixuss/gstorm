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
void pMain(in V2P i, out float4 oColor : SV_Target) {
#if 1
	float4 off = float4(sampleOffset, -sampleOffset);
	float4 col = 0;
	col += blitTex.SampleLevel(samplerState, i.uv + off.xy, 0);
	col += blitTex.SampleLevel(samplerState, i.uv + off.xw, 0);
	col += blitTex.SampleLevel(samplerState, i.uv + off.zy, 0);
	col += blitTex.SampleLevel(samplerState, i.uv + off.zw, 0);
	oColor = col * 0.25;
#else
	oColor = blitTex.SampleLevel(samplerState, i.uv, 0);
#endif
}