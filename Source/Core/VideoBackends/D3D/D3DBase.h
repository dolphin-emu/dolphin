// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <vector>

#include "Common/Common.h"

namespace DX11
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
#define SAFE_DELETE(x) { delete (x); (x) = nullptr; }
#define SAFE_DELETE_ARRAY(x) { delete[] (x); (x) = nullptr; }
#define CHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

class D3DTexture2D;

namespace D3D
{

// Wrap a ID3D11DeviceContext to prune redundant state changes.
class WrapDeviceContext {
	ID3D11DeviceContext*     ctx_ { nullptr };

	ID3D11PixelShader *      cachePS_ {};
	ID3D11GeometryShader *   cacheGS_ {};
	ID3D11VertexShader *     cacheVS_ {};
	D3D11_PRIMITIVE_TOPOLOGY cacheTopology_ { (D3D11_PRIMITIVE_TOPOLOGY) -1 };
	ID3D11InputLayout *      cacheIL_ {};

	ID3D11Buffer *           cacheIndexBuffer_ {};
	DXGI_FORMAT              cacheIndexBufferFormat_ { DXGI_FORMAT( -1 ) };
	UINT                     cacheIndexBufferOffset_ { UINT( -1 ) };

	ID3D11Buffer *           cacheVertexBuffer {};
	UINT                     cacheVBStride { UINT( -1 ) };
	UINT                     cacheVBOffset { UINT( -1 ) };

public:
	// helpers to keep wrapper transparent to user code
	WrapDeviceContext*     operator->( ) { return this; }
	ID3D11DeviceContext**  operator&( ) { return &ctx_; }
	void                   operator=( std::nullptr_t ) { ctx_ = nullptr; }

	explicit operator bool() const { return ctx_ != nullptr; }
	operator ID3D11DeviceChild* ( ) { return ctx_; }

	void ClearState() {
		cachePS_ = nullptr;
		cacheGS_ = nullptr;
		cacheVS_ = nullptr;
		cacheTopology_ = (D3D11_PRIMITIVE_TOPOLOGY) -1;
		cacheIL_ = nullptr;
		cacheIndexBuffer_ = nullptr;
		cacheIndexBufferFormat_ = DXGI_FORMAT( -1 );
		cacheIndexBufferOffset_ = UINT( -1 );
		cacheVertexBuffer = nullptr;
		cacheVBStride = UINT( -1 );
		cacheVBOffset = UINT( -1 );
		ctx_->ClearState();
	}

	void Unmap( ID3D11Resource *pResource, UINT Subresource ) { ctx_->Unmap( pResource, Subresource ); }

	HRESULT Map( ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource ) {
		return ctx_->Map( pResource, Subresource, MapType, MapFlags, pMappedResource );
	}

	void OMSetBlendState( ID3D11BlendState *pBlendState, const FLOAT BlendFactor[ 4 ], UINT SampleMask ) {
		ctx_->OMSetBlendState( pBlendState, BlendFactor, SampleMask );
	}

	void OMSetDepthStencilState( ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef ) {
		ctx_->OMSetDepthStencilState( pDepthStencilState, StencilRef );
	}

	void OMSetRenderTargets( UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView ) {
		ctx_->OMSetRenderTargets( NumViews, ppRenderTargetViews, pDepthStencilView );
	}

	void RSSetState( ID3D11RasterizerState *pRasterizerState ) {
		ctx_->RSSetState( pRasterizerState );
	}

	void RSSetViewports( UINT NumViewports, const D3D11_VIEWPORT *pViewports ) {
		ctx_->RSSetViewports( NumViewports, pViewports );
	}
	void ClearDepthStencilView( ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil ) {
		ctx_->ClearDepthStencilView( pDepthStencilView, ClearFlags, Depth, Stencil );
	}
	void ClearRenderTargetView( ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[ 4 ] ) {
		ctx_->ClearRenderTargetView( pRenderTargetView, ColorRGBA );
	}

	void DrawIndexed( UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation ) {
		ctx_->DrawIndexed( IndexCount, StartIndexLocation, BaseVertexLocation );
	}

	void IASetIndexBuffer( ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset ) {
		if ( cacheIndexBuffer_ != pIndexBuffer || cacheIndexBufferFormat_ != Format || cacheIndexBufferOffset_ != Offset ) {
			cacheIndexBuffer_ = pIndexBuffer;
			cacheIndexBufferFormat_ = Format;
			cacheIndexBufferOffset_ = Offset;
			ctx_->IASetIndexBuffer( pIndexBuffer, Format, Offset );
		}
	}

	void IASetInputLayout( ID3D11InputLayout *pInputLayout ) {
		if ( pInputLayout != cacheIL_ ) {
			cacheIL_ = pInputLayout;
			ctx_->IASetInputLayout( pInputLayout );
		}
	}

	void IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY Topology ) {
		if ( cacheTopology_ != Topology ) {
			cacheTopology_ = Topology;
			ctx_->IASetPrimitiveTopology( Topology );
		}
	}

	void IASetVertexBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets ) {
		if ( cacheVertexBuffer != *ppVertexBuffers || cacheVBStride != *pStrides || cacheVBOffset != *pOffsets ) {
			cacheVertexBuffer = *ppVertexBuffers;
			cacheVBStride = *pStrides;
			cacheVBOffset = *pOffsets;
			ctx_->IASetVertexBuffers( StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets );
		}
	}

	void PSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		ctx_->PSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
	}

	void PSSetSamplers( UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers ){
		ctx_->PSSetSamplers( StartSlot, NumSamplers, ppSamplers );
	}

	void PSSetShader( ID3D11PixelShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != cachePS_ ) {
			cachePS_ = pShader;
			ctx_->PSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}

	void UpdateSubresource( ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch ){
		ctx_->UpdateSubresource( pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch );
	}

	void GSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		ctx_->GSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
	}

	void GSSetShader( ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != cacheGS_ ) {
			cacheGS_ = pShader;
			ctx_->GSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}

	void Flush() { ctx_->Flush(); }
	ULONG Release() { return ctx_->Release(); }

	void Begin( ID3D11Asynchronous *pAsync ) { ctx_->Begin( pAsync ); }
	void End( ID3D11Asynchronous *pAsync ) { ctx_->End( pAsync ); }

	HRESULT GetData( ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags ) {
		return ctx_->GetData( pAsync, pData, DataSize, GetDataFlags );
	}

	void VSSetShader( ID3D11VertexShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != cacheVS_ ) {
			cacheVS_ = pShader;
			ctx_->VSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}
	void VSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		ctx_->VSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
	}

	void Draw( UINT VertexCount, UINT StartVertexLocation ) {
		ctx_->Draw( VertexCount, StartVertexLocation );
	}

	void PSSetShaderResources( UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews ) {
		ctx_->PSSetShaderResources( StartSlot, NumViews, ppShaderResourceViews );
	}

	void ResolveSubresource( ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format ) {
		ctx_->ResolveSubresource( pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format );
	}

	void CopySubresourceRegion( ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox ) {
		ctx_->CopySubresourceRegion( pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox );
	}

	void CopyResource( ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource ) {
		ctx_->CopyResource( pDstResource, pSrcResource );
	}

	void RSSetScissorRects( UINT NumRects, const D3D11_RECT *pRects ) {
		ctx_->RSSetScissorRects( NumRects, pRects );
	}
};

HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DCompiler();

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter);
std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);
DXGI_SAMPLE_DESC GetAAMode(int index);

HRESULT Create(HWND wnd);
void Close();

extern ID3D11Device* device;
extern WrapDeviceContext context;
extern IDXGISwapChain* swapchain;
extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* &GetBackBuffer();
const char* PixelShaderVersionString();
const char* GeometryShaderVersionString();
const char* VertexShaderVersionString();
bool BGRATexturesSupported();

unsigned int GetMaxTextureSize();

// Ihis function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
template <typename T>
void SetDebugObjectName(T resource, const char* name)
{
	static_assert(std::is_convertible<T, ID3D11DeviceChild*>::value,
		"resource must be convertible to ID3D11DeviceChild*");
#if defined(_DEBUG) || defined(DEBUGFAST)
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#endif
}

}  // namespace D3D

typedef HRESULT (WINAPI *CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY PCreateDXGIFactory;
typedef HRESULT (WINAPI *D3D11CREATEDEVICE)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
extern D3D11CREATEDEVICE PD3D11CreateDevice;

typedef HRESULT (WINAPI *D3DREFLECT)(LPCVOID, SIZE_T, REFIID, void**);
extern D3DREFLECT PD3DReflect;
extern pD3DCompile PD3DCompile;

}  // namespace DX11
