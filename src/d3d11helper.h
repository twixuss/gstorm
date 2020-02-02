#pragma once
#include "winhelper.h"
#include "common.h"
#include "math.h"
#include <dxgi1_4.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "../dep/Microsoft DirectX SDK/Include/D3DX11.h"
#include <atomic>
#include <future>
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "../dep/Microsoft DirectX SDK/Lib/x64/d3dx11")
#ifdef BUILD_RELEASE
#define CREATE_DEVICE_FLAGS 0
#else 
#define CREATE_DEVICE_FLAGS D3D11_CREATE_DEVICE_DEBUG
#endif
struct D3D11 {
	struct RenderTarget {
		ID3D11RenderTargetView* rt = 0;
		ID3D11ShaderResourceView* sr = 0;
		ID3D11Texture2D* tex = 0;
		RenderTarget() = default;
		RenderTarget(const RenderTarget& other) { *this = other; }
		RenderTarget(RenderTarget&& other) { *this = std::move(other); }
		RenderTarget& operator=(const RenderTarget&) = default;
		RenderTarget& operator=(RenderTarget&& other) {
			if (tex) {
				release();
			}
			rt = other.rt;
			sr = other.sr;
			tex = other.tex;
			other.zero();
			return *this;
		}
		~RenderTarget() {
			if (tex) {
				release();
				zero();
			}
		}
	private:
		void release() {
			tex->Release();
			rt->Release();
			sr->Release();
		}
		void zero() {
			tex = 0;
			rt = 0;
			sr = 0;
		}
	};
	struct DepthStencilView {
		ID3D11DepthStencilView* view = 0;
		void release() {
			if (view) {
				view->Release();
				view = 0;
			}
		}
	};
	template<class Data>
	struct ConstantBuffer : Data {
		static_assert(alignof(Data) % 16 == 0, "Data must be aligned on 16 byte boundary");
		ID3D11Buffer* buffer = 0;
		void create(D3D11& renderer, D3D11_USAGE usage) {
			switch (usage) {
				case D3D11_USAGE_DYNAMIC:
					buffer = renderer.createDynamicBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(Data), 0, 0, this);
					break;
				case D3D11_USAGE_IMMUTABLE:
					buffer = renderer.createImmutableBuffer(D3D11_BIND_CONSTANT_BUFFER, sizeof(Data), 0, 0, this);
					break;
				default:
					assert(0);
			}
		}
		void update(D3D11& renderer) {
			renderer.updateDynamicBuffer(buffer, this, sizeof(Data));
		}
	};
	D3D11(HWND window) {
		DHR(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

		for (UINT i = 0; factory->EnumAdapters(i, (IDXGIAdapter**)&adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				adapter->Release();
				adapter = 0;
				continue;
			}
			wprintf(L"Found adapter: %s\n", desc.Description);
			break;
		}
		if (!adapter) {
			puts("No adapter");
			assert(0);
		}

		DHR(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, 0, CREATE_DEVICE_FLAGS, 0, 0, D3D11_SDK_VERSION, &device, 0, &deviceContext));

		DXGI_SWAP_CHAIN_DESC swapChainDesc {};
		swapChainDesc.BufferCount = 1;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.Width = 1;  // <<-- this doesn't matter because WM_SIZE message is sent on startup
		swapChainDesc.BufferDesc.Height = 1; // <<--
		swapChainDesc.BufferDesc.RefreshRate = {60, 1};
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = window;
		swapChainDesc.SampleDesc = {1, 0};
		swapChainDesc.Windowed = 1;

		DHR(factory->CreateSwapChain(device, &swapChainDesc, &swapChain));

		//DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, CREATE_DEVICE_FLAGS,
		//								  0, 0, D3D11_SDK_VERSION, &swapChainDesc,
		//								  &swapChain, &device, 0, &deviceContext));
	}
	void resize(ID3D11RenderTargetView*& backBuffer, V2i size) {
		if (backBuffer) {
			backBuffer->Release();
		}
		DHR(swapChain->ResizeBuffers(1, size.x, size.y, DXGI_FORMAT_UNKNOWN, 0));
		ID3D11Texture2D* backBufferTexture = 0;
		DHR(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture)));
		DHR(device->CreateRenderTargetView(backBufferTexture, 0, &backBuffer));
		backBufferTexture->Release();
	}
	void setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY t) {
		deviceContext->IASetPrimitiveTopology(t);
	}
	DepthStencilView createDepthStencilView(V2i size) {
		DepthStencilView result;
		D3D11_TEXTURE2D_DESC desc {};
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.Width = size.x;
		desc.Height = size.y;
		desc.MipLevels = 1;
		desc.SampleDesc = {1, 0};
		ID3D11Texture2D* depthTex = 0;
		DHR(device->CreateTexture2D(&desc, 0, &depthTex));
		DHR(device->CreateDepthStencilView(depthTex, 0, &result.view));
		depthTex->Release();
		return result;
	}
	void clearRenderTargetView(ID3D11RenderTargetView* r, V4 c) {
		deviceContext->ClearRenderTargetView(r, c.data());
	}
	void clearDepthStencilView(ID3D11DepthStencilView* v, UINT f, FLOAT d, UINT8 s) {
		deviceContext->ClearDepthStencilView(v, f, d, s);
	}
	void setRenderTarget(ID3D11RenderTargetView* r, ID3D11DepthStencilView* d) {
		deviceContext->OMSetRenderTargets(1, &r, d);
	}
	size_t getVRAM() {
		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);

		return videoMemoryInfo.CurrentUsage;
	}
	u32 getDrawCalls() {
		return std::exchange(drawCalls, 0);
	}
	void draw(u32 vertexCount, u32 offset = 0) {
		deviceContext->Draw(vertexCount, offset);
		++drawCalls;
	}
	ID3D11VertexShader* createVertexShader(wchar_t const* path, const D3D_SHADER_MACRO* defines = 0) {
		ID3DBlob* blob = 0;
		DHR(compileShader(path, defines, "vMain", "vs_5_0", &blob));
		ID3D11VertexShader* vertexShader;
		DHR(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &vertexShader));
		blob->Release();
		return vertexShader;
	}
	ID3D11PixelShader* createPixelShader(wchar_t const* path, const D3D_SHADER_MACRO* defines = 0) {
		ID3DBlob* blob = 0;
		DHR(compileShader(path, defines, "pMain", "ps_5_0", &blob));
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
	void vsSetShaderResource(UINT slot, ID3D11ShaderResourceView* srv) { deviceContext->VSSetShaderResources(slot, 1, &srv); }
	void psSetShaderResource(UINT slot, ID3D11ShaderResourceView* srv) { deviceContext->PSSetShaderResources(slot, 1, &srv); }
	void vsSetShader(ID3D11VertexShader* s) { deviceContext->VSSetShader(s, 0, 0); }
	void psSetShader(ID3D11PixelShader* s) { deviceContext->PSSetShader(s, 0, 0); }
	void psSetSampler(UINT slot, ID3D11SamplerState* s) { deviceContext->PSSetSamplers(slot, 1, &s); }
	void bindConstantBuffersV(UINT slot, std::initializer_list<ID3D11Buffer*> buffers) {
		assert(buffers.size() <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
		deviceContext->VSSetConstantBuffers(0, (UINT)buffers.size(), buffers.begin());
	}
	void bindConstantBuffersP(UINT slot, std::initializer_list<ID3D11Buffer*> buffers) {
		assert(buffers.size() <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
		deviceContext->PSSetConstantBuffers(0, (UINT)buffers.size(), buffers.begin());
	}
	template<class... T> void bindConstantBuffersV(UINT slot, ConstantBuffer<T>&... buffers) { bindConstantBuffersV(slot, {(buffers.buffer)...}); }
	template<class... T> void bindConstantBuffersP(UINT slot, ConstantBuffer<T>&... buffers) { bindConstantBuffersP(slot, {(buffers.buffer)...}); }
	void updateDefaultBuffer(ID3D11Resource* buffer, void const* data, UINT dataSize) {
		deviceContext->UpdateSubresource(buffer, 0, 0, data, dataSize, 1);
	}
	template<class T>
	void updateDefaultBuffer(ID3D11Resource* buffer, T const& data) {
		updateDefaultBuffer(buffer, &data, sizeof(data));
	}
	void updateDynamicBuffer(ID3D11Resource* buffer, void const* data, size_t dataSize) {
		D3D11_MAPPED_SUBRESOURCE mapped {};
		DHR(deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, data, dataSize);
		deviceContext->Unmap(buffer, 0);
	}
	template<class T>
	void updateDynamicBuffer(ID3D11Resource* buffer, T const& data) {
		updateDynamicBuffer(buffer, &data, sizeof(data));
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
	ID3D11ShaderResourceView* loadTexture(char const* path) {
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
	ID3D11ShaderResourceView* createShaderResourceView(ID3D11Resource* resource, D3D11_SRV_DIMENSION dimension, UINT numElements) {
		D3D11_SHADER_RESOURCE_VIEW_DESC desc {};
		desc.ViewDimension = dimension;
		desc.Buffer.NumElements = numElements;
		ID3D11ShaderResourceView* result;
		DHR(device->CreateShaderResourceView(resource, &desc, &result));
		return result;
	}
	void present(UINT sync) {
		DHR(swapChain->Present(sync, 0));
	}
	auto loadTextureAsync(std::string&& path) {
		return std::async(std::launch::async, [this, path = std::move(path)] {
			return loadTexture(path.data());
		});
	}
	auto loadTextureAsync(const char* path) {
		return loadTextureAsync(std::string(path));
	}
	auto createVertexShaderAsync(std::wstring path, std::vector<D3D_SHADER_MACRO> macros = {{}}) {
		return std::async(std::launch::async, [this, path = std::move(path), macros = std::move(macros)] {
			return createVertexShader(path.data(), macros.data());
		});
	}
	auto createPixelShaderAsync(std::wstring path, std::vector<D3D_SHADER_MACRO> macros = {{}}) {
		return std::async(std::launch::async, [this, path = std::move(path), macros = std::move(macros)] {
			return createPixelShader(path.data(), macros.data());
		});
	}
private:
	IDXGIFactory* factory;
	IDXGIAdapter3* adapter;
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	u32 drawCalls = 0;
	HRESULT compileShader(wchar_t const* path, const D3D_SHADER_MACRO* defines, char const* entry, char const* profile, ID3DBlob** blob) {
		ID3DBlob* errors = 0;
		HRESULT hr = D3DCompileFromFile(path, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, profile,
										D3DCOMPILE_OPTIMIZATION_LEVEL3,
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