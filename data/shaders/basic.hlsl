cbuffer _ : register(b0) {
	matrix mvp;
	float4 solidColor;
}
struct V2P {
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : UV;
};
struct Vertex {
	float3 position;
	float3 normal;
	float2 uv;
};
StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);
Texture2D grassTex : register(t2);
SamplerState samplerState : register(s0);
#define MAP(type)                                       \
type map(type v, type si, type sa, type di, type da) {  \
	return (v - si) / (sa - si) * (da - di) + di;       \
}

MAP(float)
MAP(float2)
MAP(float3)
MAP(float4)

void vMain(in uint id : SV_VertexID, out V2P o) {
	Vertex v = vertices[indices[id]];
	o.position = mul(mvp, float4(v.position, 1));
	o.uv = v.uv;
	o.uv.y = 1 - o.uv.y;
	o.color = solidColor * abs(dot(v.normal, normalize(float3(3,5,4))));
}
void pMain(in V2P i, out float4 oColor : SV_Target) {
	oColor = grassTex.Sample(samplerState, i.uv) * i.color;
	//oColor = float4(i.uv, 0, 1) * i.color;
}