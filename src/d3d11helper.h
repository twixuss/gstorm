#pragma once
#include "common.h"
#include "math.h"
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "../dep/Microsoft DirectX SDK/Include/D3DX11.h"
#include <atomic>
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "../dep/Microsoft DirectX SDK/Lib/x64/d3dx11")
#define DHR(call) 	      \
do {				      \
	HRESULT hr = call;    \
	assert(SUCCEEDED(hr));\
} while (0)
#ifdef BUILD_RELEASE
#define CREATE_DEVICE_FLAGS 0
#else 
#define CREATE_DEVICE_FLAGS D3D11_CREATE_DEVICE_DEBUG
#endif
struct RenderTarget {
	ID3D11RenderTargetView* rt = 0;
	ID3D11ShaderResourceView* sr = 0;
	ID3D11Texture2D* tex = 0;
	void Release() {
		tex->Release();
		rt->Release();
		sr->Release();
		tex = 0;
		rt = 0;
		sr = 0;
	}
	operator bool() {
		return tex;
	}
};
struct Renderer {
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	std::atomic_uint drawCalls;
	Renderer(HWND window, V2i size) {
		DXGI_SWAP_CHAIN_DESC swapChainDesc {};
		swapChainDesc.BufferCount = 2;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.Width =size.x;
		swapChainDesc.BufferDesc.Height = size.y;
		swapChainDesc.BufferDesc.RefreshRate = {60, 1};
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = window;
		swapChainDesc.SampleDesc = {1, 0};
		swapChainDesc.Windowed = 1;

		DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, CREATE_DEVICE_FLAGS,
										  0, 0, D3D11_SDK_VERSION, &swapChainDesc,
										  &swapChain, &device, 0, &deviceContext));
	}
	u32 getDrawCalls() {
		return drawCalls.exchange(0);
	}
	void draw(u32 vertexCount, u32 offset = 0) {
		deviceContext->Draw(vertexCount, offset);
		++drawCalls;
	}
	ID3D11VertexShader* createVertexShader(wchar_t const* path) {
		ID3DBlob* blob = 0;
		DHR(compileShader(path, "vMain", "vs_5_0", &blob));
		ID3D11VertexShader* vertexShader;
		DHR(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &vertexShader));
		blob->Release();
		return vertexShader;
	}
	ID3D11PixelShader* createPixelShader(wchar_t const* path) {
		ID3DBlob* blob = 0;
		DHR(compileShader(path, "pMain", "ps_5_0", &blob));
		ID3D11PixelShader* pixelShader;
		DHR(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &pixelShader));
		blob->Release();
		return pixelShader;
	}
	void setViewport(FLOAT x, FLOAT y, FLOAT width, FLOAT height) {
		D3D11_VIEWPORT viewport {};
		viewport.TopLeftX = x;
		viewport.TopLeftY = y;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &viewport);
	}
	void setViewport(int x, int y, int width, int height) {
		setViewport((FLOAT)x, (FLOAT)y, (FLOAT)width, (FLOAT)height);
	}
	void setViewport(int width, int height) {
		setViewport(0, 0, width, height);
	}
	void setViewport(V2i size) {
		setViewport(size.x, size.y);
	}
	void setViewport(V2i pos, V2i size) {
		setViewport(pos.x, pos.y, size.x, size.y);
	}
	ID3D11Buffer* createBuffer(D3D11_BIND_FLAG bindFlag, UINT size, UINT cpuAccess, UINT misc, UINT stride, D3D11_USAGE usage, void const* initialData = 0) {
		D3D11_BUFFER_DESC desc {};
		desc.BindFlags = bindFlag;
		desc.ByteWidth = size;
		desc.CPUAccessFlags = cpuAccess;
		desc.MiscFlags = misc;
		desc.StructureByteStride = stride;
		desc.Usage = usage;
		D3D11_SUBRESOURCE_DATA data {};
		data.pSysMem = initialData;
		ID3D11Buffer* buffer;
		DHR(device->CreateBuffer(&desc, initialData ? &data : 0, &buffer));
		return buffer;
	}
	ID3D11Buffer* createDynamicBuffer(D3D11_BIND_FLAG bindFlag, UINT size, UINT misc, UINT stride, void const* initialData = 0) {
		return createBuffer(bindFlag, size, D3D11_CPU_ACCESS_WRITE, misc, stride, D3D11_USAGE_DYNAMIC, initialData);
	}
	ID3D11Buffer* createImmutableBuffer(D3D11_BIND_FLAG bindFlag, UINT size, UINT misc, UINT stride, void const* initialData) {
		return createBuffer(bindFlag, size, 0, misc, stride, D3D11_USAGE_IMMUTABLE, initialData);
	}
	ID3D11Buffer* createStructuredBuffer(UINT size, UINT cpuAccess, UINT stride, D3D11_USAGE usage, void const* initialData = 0) {
		return createBuffer(D3D11_BIND_SHADER_RESOURCE, size, cpuAccess, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, stride, usage, initialData);
	}
	ID3D11Buffer* createImmutableStructuredBuffer(UINT size, UINT stride, void const* initialData = 0) {
		return createStructuredBuffer(size, 0, stride, D3D11_USAGE_IMMUTABLE, initialData);
	}
	ID3D11ShaderResourceView* createBufferView(ID3D11Buffer* buffer, UINT numElements) {
		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = numElements;
		ID3D11ShaderResourceView* bufferView;
		DHR(device->CreateShaderResourceView(buffer, &desc, &bufferView));
		return bufferView;
	}
	void updateBuffer(ID3D11Resource* buffer, void const* data, size_t dataSize) {
		D3D11_MAPPED_SUBRESOURCE mapped {};
		DHR(deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, data, dataSize);
		deviceContext->Unmap(buffer, 0);
	}
	template<class T>
	void updateBuffer(ID3D11Resource* buffer, T const& data) {
		updateBuffer(buffer, &data, sizeof(data));
	}
	ID3D11SamplerState* createSamplerState(D3D11_TEXTURE_ADDRESS_MODE u, D3D11_TEXTURE_ADDRESS_MODE v, D3D11_TEXTURE_ADDRESS_MODE w, D3D11_FILTER filter, f32 maxLod = FLT_MAX) {
		D3D11_SAMPLER_DESC desc {};
		desc.AddressU = u;
		desc.AddressV = v;
		desc.AddressW = w;
		desc.Filter = filter;
		desc.MaxAnisotropy = 16;
		desc.MaxLOD = maxLod;
		ID3D11SamplerState* samplerState;
		DHR(device->CreateSamplerState(&desc, &samplerState));
		return samplerState;
	}
	ID3D11SamplerState* createSamplerState(D3D11_TEXTURE_ADDRESS_MODE address, D3D11_FILTER filter, f32 maxLod = FLT_MAX) {
		return createSamplerState(address, address, address, filter, maxLod);
	}
	RenderTarget createRenderTarget(V2i size) {
		RenderTarget result;
		{
			D3D11_TEXTURE2D_DESC desc {};
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.Width = size.x;
			desc.Height = size.y;
			desc.MipLevels = 1;
			desc.MiscFlags = 0;
			desc.SampleDesc = {1, 0};
			desc.Usage = D3D11_USAGE_DEFAULT;
			DHR(device->CreateTexture2D(&desc, 0, &result.tex));
		}
		D3D11_RENDER_TARGET_VIEW_DESC desc {};
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		DHR(device->CreateRenderTargetView(result.tex, &desc, &result.rt));
		DHR(device->CreateShaderResourceView(result.tex, 0, &result.sr));
		return result;
	}
	ID3D11ShaderResourceView* createTexture(char const* path) {
		ID3D11ShaderResourceView* result;
		DHR(D3DX11CreateShaderResourceViewFromFileA(device, path, 0, 0, &result, 0));
		return result;
	}
	ID3D11BlendState* createBlendState(D3D11_BLEND_OP op, D3D11_BLEND src, D3D11_BLEND dst, D3D11_BLEND_OP opa, D3D11_BLEND srca, D3D11_BLEND dsta) {
		D3D11_BLEND_DESC desc {};
		desc.RenderTarget[0].BlendEnable = 1;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		desc.RenderTarget[0].BlendOp = op;
		desc.RenderTarget[0].SrcBlend = src;
		desc.RenderTarget[0].DestBlend = dst;
		desc.RenderTarget[0].BlendOpAlpha = opa;
		desc.RenderTarget[0].SrcBlendAlpha = srca;
		desc.RenderTarget[0].DestBlendAlpha = dsta;
		ID3D11BlendState* result;
		DHR(device->CreateBlendState(&desc, &result));
		return result;
	}
	ID3D11BlendState* createBlendState(D3D11_BLEND_OP op, D3D11_BLEND src, D3D11_BLEND dst) {
		return createBlendState(op, src, dst, D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO);
	}

private:
	HRESULT compileShader(wchar_t const* path, char const* entry, char const* profile, ID3DBlob** blob) {
		ID3DBlob* errors = 0;
		HRESULT hr = D3DCompileFromFile(path, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, profile,
										D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_WARNINGS_ARE_ERRORS, 
										0, blob, &errors);
		if (errors) {
			puts("Compilation failed");
			_putws(path);
			puts((char*)errors->GetBufferPointer());
			errors->Release();
		}
		return hr;
	}
};