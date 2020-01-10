#pragma once
#include "common.h"
#include "math.h"
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "../dep/Microsoft DirectX SDK/Include/D3DX11.h"
#pragma comment(lib, "d3d11")
#pragma comment(lib, "../dep/Microsoft DirectX SDK/Lib/x64/d3dx11")
#ifdef BUILD_RELEASE
#define DHR(call) call
#define CREATE_DEVICE_FLAGS 0
#else 
#define DHR(call) 	   \
do {				   \
	HRESULT hr = call; \
	assert(SUCCEEDED(hr));   \
} while (0)
#define CREATE_DEVICE_FLAGS D3D11_CREATE_DEVICE_DEBUG
#endif
HRESULT compileShader(const char* path, const char* entry, const char* profile, ID3DBlob** blob) {
	ID3DBlob* errors = 0;
	HRESULT hr = D3DX11CompileFromFileA(path, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, profile, 0, 0, 0, blob, &errors, 0);
	if (errors) {
		puts("Compilation failed");
		puts(path);
		puts((char*)errors->GetBufferPointer());
		errors->Release();
	}
	return hr;
}
void createVertexShader(ID3D11Device* device, const char* path, ID3D11VertexShader** vertexShader) {
	ID3DBlob* blob = 0;
	DHR(compileShader(path, "vMain", "vs_5_0", &blob));
	DHR(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, vertexShader));
	blob->Release();
}
void createPixelShader(ID3D11Device* device, const char* path, ID3D11PixelShader** pixelShader) {
	ID3DBlob* blob = 0;
	DHR(compileShader(path, "pMain", "ps_5_0", &blob));
	DHR(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, pixelShader));
	blob->Release();
}
void setViewport(ID3D11DeviceContext* deviceContext, FLOAT width, FLOAT height) {
	D3D11_VIEWPORT viewport {};
	viewport.Width = width;
	viewport.Height = height;
	viewport.MaxDepth = 1.0f;
	deviceContext->RSSetViewports(1, &viewport);
}
void setViewport(ID3D11DeviceContext* deviceContext, int width, int height) {
	setViewport(deviceContext, (FLOAT)width, (FLOAT)height);
}
void setViewport(ID3D11DeviceContext* deviceContext, V2i size) {
	setViewport(deviceContext, size.x, size.y);
}
void createBuffer(ID3D11Device* device, D3D11_BIND_FLAG bindFlag, UINT size, UINT cpuAccess, UINT misc, UINT stride, D3D11_USAGE usage, ID3D11Buffer** buffer, void const* initialData = 0) {
	D3D11_BUFFER_DESC desc {};
	desc.BindFlags = bindFlag;
	desc.ByteWidth = size;
	desc.CPUAccessFlags = cpuAccess;
	desc.MiscFlags = misc;
	desc.StructureByteStride = stride;
	desc.Usage = usage;
	D3D11_SUBRESOURCE_DATA data {};
	data.pSysMem = initialData;
	DHR(device->CreateBuffer(&desc, initialData ? &data : 0, buffer));
}
void createDynamicBuffer(ID3D11Device* device, D3D11_BIND_FLAG bindFlag, UINT size, UINT misc, UINT stride, ID3D11Buffer** buffer, void const* initialData = 0) {
	createBuffer(device, bindFlag, size, D3D11_CPU_ACCESS_WRITE, misc, stride, D3D11_USAGE_DYNAMIC, buffer, initialData);
}
void createConstBuffer(ID3D11Device* device, D3D11_BIND_FLAG bindFlag, UINT size, UINT misc, UINT stride, ID3D11Buffer** buffer, void const* initialData) {
	createBuffer(device, bindFlag, size, 0, misc, stride, D3D11_USAGE_IMMUTABLE, buffer, initialData);
}
void createStructuredBuffer(ID3D11Device* device, UINT size, UINT cpuAccess, UINT stride, D3D11_USAGE usage, ID3D11Buffer** buffer, void const* initialData = 0) {
	createBuffer(device, D3D11_BIND_SHADER_RESOURCE, size, cpuAccess, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, stride, usage, buffer, initialData);
}
void createConstStructuredBuffer(ID3D11Device* device, UINT size, UINT stride, ID3D11Buffer** buffer, void const* initialData = 0) {
	createStructuredBuffer(device, size, 0, stride, D3D11_USAGE_IMMUTABLE, buffer, initialData);
}
void createBufferView(ID3D11Device* device, ID3D11Buffer* buffer, UINT numElements, ID3D11ShaderResourceView** bufferView) {
	D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.NumElements = numElements;
	DHR(device->CreateShaderResourceView(buffer, &desc, bufferView));
}
void updateBuffer(ID3D11DeviceContext* deviceContext, ID3D11Resource* buffer, void const* data, size_t dataSize) {
	D3D11_MAPPED_SUBRESOURCE mapped {};
	DHR(deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, data, dataSize);
	deviceContext->Unmap(buffer, 0);
}
template<class T>
void updateBuffer(ID3D11DeviceContext* deviceContext, ID3D11Resource* buffer, T const& data) {
	updateBuffer(deviceContext, buffer, &data, sizeof(data));
}