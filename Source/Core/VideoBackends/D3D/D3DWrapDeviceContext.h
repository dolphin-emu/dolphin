// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <array>

namespace DX11
{

namespace D3D
{

// Wrap a ID3D11DeviceContext to prune redundant state changes.
class WrapDeviceContext {
	ID3D11DeviceContext*     ctx_ { nullptr };

	struct Cache {
		ID3D11PixelShader *      ps_{};
		ID3D11GeometryShader *   gs_{};
		ID3D11VertexShader *     vs_{};
		D3D11_PRIMITIVE_TOPOLOGY topology_= D3D11_PRIMITIVE_TOPOLOGY(-1);
		ID3D11InputLayout *      il_{};

		ID3D11Buffer *           ib_{};
		DXGI_FORMAT              ibFormat_=DXGI_FORMAT( -1 );
		UINT                     ibOffset_=UINT( -1 );

		// only cache first vb
		ID3D11Buffer *           vb_{};
		UINT                     vbStride_=UINT( -1 );
		UINT                     vbOffset_=UINT( -1 );

		ID3D11RasterizerState *  rasterizerState_{};

		ID3D11BlendState *       blendState_{};
		std::array<FLOAT,4>      blendFactor_ { std::array<FLOAT,4>{ 1, 1, 1, 1} }; // VS2013 miss non static aggregate member c++11
		UINT                     sampleMask_=0xffffffffu;

		ID3D11DepthStencilState * depthStencilState_{};
		UINT                      stencilRef_{};

		std::array<ID3D11ShaderResourceView*,8> psSrvs_;
		std::array<bool,8>                      psSrvDirties_;
		bool                                    psSrvDirty_;
	 
		std::array<ID3D11SamplerState*,8>       samplerStates_;
		std::array<bool,8>                      samplerStateDirties_;
		bool                                    samplerStateDirty_;

		// only manage one constant buffer
		ID3D11Buffer* vsCb0_{};
		ID3D11Buffer* psCb0_{};

		Cache() {
			psSrvs_.fill(nullptr);
			psSrvDirties_.fill(false);
			samplerStates_.fill(nullptr);
			samplerStateDirties_.fill(false);
		}
	};

	Cache c_;

public:

	// helpers to keep wrapper transparent to user code
	WrapDeviceContext*     operator->( ) { return this; }
	ID3D11DeviceContext**  operator&( ) { return &ctx_; }
	void                   operator=( std::nullptr_t ) { ctx_ = nullptr; }
	explicit operator bool() const { return ctx_ != nullptr; }
	operator ID3D11DeviceChild* ( ) { return ctx_; }

	//
	ULONG Release() { 
		c_ = Cache{}; // in case of restart, as i am a global variable.
		return ctx_->Release(); 
	}

	void Flush() { 
		ctx_->Flush(); 
	}

	//
	void Unmap( ID3D11Resource *pResource, UINT Subresource ) { 
		ctx_->Unmap( pResource, Subresource ); 
	}
	HRESULT Map( ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource ) {
		return ctx_->Map( pResource, Subresource, MapType, MapFlags, pMappedResource );
	}

	//
	void ClearState() {
		c_ = Cache{};
		ctx_->ClearState();
	}

	void OMSetBlendState( ID3D11BlendState *pBlendState, const FLOAT BlendFactor[ 4 ], UINT SampleMask ) {
		static float const defaultBlendFactors[4]{1,1,1,1};
		if( c_.blendState_ != pBlendState || c_.sampleMask_ != SampleMask || memcmp(c_.blendFactor_.data(), BlendFactor? BlendFactor:defaultBlendFactors, sizeof(c_.blendFactor_) ) ) {
			c_.blendState_ = pBlendState;
			c_.sampleMask_ = SampleMask;
			memcpy(c_.blendFactor_.data(), BlendFactor? BlendFactor:defaultBlendFactors, sizeof(c_.blendFactor_) );
			ctx_->OMSetBlendState( pBlendState, BlendFactor, SampleMask );
		}
	}

	void OMSetDepthStencilState( ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef ) {
		if( c_.depthStencilState_!=pDepthStencilState || c_.stencilRef_!=StencilRef) {
			c_.depthStencilState_=pDepthStencilState;
			c_.stencilRef_=StencilRef;
			ctx_->OMSetDepthStencilState( pDepthStencilState, StencilRef );
		}
	}

	void RSSetState( ID3D11RasterizerState *pRasterizerState ) {
		if ( c_.rasterizerState_ != pRasterizerState ) {
			c_.rasterizerState_ = pRasterizerState;
			ctx_->RSSetState( pRasterizerState );
		}
	}

	void RSSetScissorRects( UINT NumRects, const D3D11_RECT *pRects ) {
		ctx_->RSSetScissorRects( NumRects, pRects );
	}

	void RSSetViewports( UINT NumViewports, const D3D11_VIEWPORT *pViewports ) {
		ctx_->RSSetViewports( NumViewports, pViewports );
	}

	void IASetIndexBuffer( ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset ) {
		if ( c_.ib_ != pIndexBuffer || c_.ibFormat_ != Format || c_.ibOffset_ != Offset ) {
			c_.ib_ = pIndexBuffer;
			c_.ibFormat_ = Format;
			c_.ibOffset_ = Offset;
			ctx_->IASetIndexBuffer( pIndexBuffer, Format, Offset );
		}
	}

	void IASetInputLayout( ID3D11InputLayout *pInputLayout ) {
		if ( pInputLayout != c_.il_ ) {
			c_.il_ = pInputLayout;
			ctx_->IASetInputLayout( pInputLayout );
		}
	}

	void IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY Topology ) {
		if ( c_.topology_ != Topology ) {
			c_.topology_ = Topology;
			ctx_->IASetPrimitiveTopology( Topology );
		}
	}
	//
	void OMSetRenderTargets( UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView ) {
		// OMSetRenderTargets will unbind SRVs if they are in conflict with the new RTVs or DSV.
		std::array<ID3D11ShaderResourceView*,8> nils{nullptr};
		PSSetShaderResources(0,8,nils.data());
		ApplyLazyStates();
		ctx_->OMSetRenderTargets( NumViews, ppRenderTargetViews, pDepthStencilView );
	}

	// TODO: assert if StartSlot!=0 || NumBuffers!=1
	void IASetVertexBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets ) {
		
		if ( c_.vb_ != *ppVertexBuffers || c_.vbStride_ != *pStrides || c_.vbOffset_ != *pOffsets ) {
			c_.vb_ = *ppVertexBuffers;
			c_.vbStride_ = *pStrides;
			c_.vbOffset_ = *pOffsets;
			ctx_->IASetVertexBuffers( StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets );
		}
	}

	// Shader states
	void PSSetShader( ID3D11PixelShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != c_.ps_ ) {
			c_.ps_ = pShader;
			ctx_->PSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}
	void GSSetShader( ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != c_.gs_ ) {
			c_.gs_ = pShader;
			ctx_->GSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}

	void VSSetShader( ID3D11VertexShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances ){
		if ( pShader != c_.vs_ ) {
			c_.vs_ = pShader;
			ctx_->VSSetShader( pShader, ppClassInstances, NumClassInstances );
		}
	}

	void VSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		if( StartSlot==0 ) {
			if ( c_.vsCb0_ != *ppConstantBuffers ) {
				c_.vsCb0_ = *ppConstantBuffers;
			} else {
				++StartSlot;
				--NumBuffers;
				++ppConstantBuffers;
			}
		}
		if( NumBuffers )
			ctx_->VSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
	}

	void PSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		if( StartSlot==0 ) {
			if ( c_.psCb0_ != *ppConstantBuffers ) {
				c_.psCb0_ = *ppConstantBuffers;
			} else {
				++StartSlot;
				--NumBuffers;
				++ppConstantBuffers;
			}
		}
		if( NumBuffers )
			ctx_->PSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
	}

	void PSSetShaderResources( UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews ) {
		NumViews+=StartSlot;
		bool dirty{};
		for( ; StartSlot != NumViews; ++StartSlot) {
			if( c_.psSrvs_[StartSlot] != *ppShaderResourceViews) {
				c_.psSrvs_[StartSlot] = *ppShaderResourceViews;
				c_.psSrvDirties_[StartSlot] = true;
				dirty = true;
			}
			++ppShaderResourceViews;
		}
		c_.psSrvDirty_ |= dirty;
	}

	void PSSetSamplers( UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers ){
		NumSamplers+=StartSlot;
		bool dirty{};
		for( ; StartSlot != NumSamplers; ++StartSlot) {
			if( c_.samplerStates_[StartSlot] != *ppSamplers) {
				c_.samplerStates_[StartSlot] = *ppSamplers;
				c_.samplerStateDirties_[StartSlot] = true;
				dirty = true;
			}
			++ppSamplers;
		}
		c_.samplerStateDirty_ |= dirty;
	}

	// Action Calls
	void ClearDepthStencilView( ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil ) {
		ctx_->ClearDepthStencilView( pDepthStencilView, ClearFlags, Depth, Stencil );
	}
	void ClearRenderTargetView( ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[ 4 ] ) {
		ctx_->ClearRenderTargetView( pRenderTargetView, ColorRGBA );
	}

	void DrawIndexed( UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation ) {
		ApplyLazyStates();
		ctx_->DrawIndexed( IndexCount, StartIndexLocation, BaseVertexLocation );
	}

	void Draw( UINT VertexCount, UINT StartVertexLocation ) {
		ApplyLazyStates();
		ctx_->Draw( VertexCount, StartVertexLocation );
	}

	void UpdateSubresource( ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch ){
		ctx_->UpdateSubresource( pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch );
	}

	void GSSetConstantBuffers( UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers ) {
		ctx_->GSSetConstantBuffers( StartSlot, NumBuffers, ppConstantBuffers );
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

	//
	void Begin( ID3D11Asynchronous *pAsync ) { ctx_->Begin( pAsync ); }
	void End( ID3D11Asynchronous *pAsync ) { ctx_->End( pAsync ); }

	HRESULT GetData( ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags ) {
		return ctx_->GetData( pAsync, pData, DataSize, GetDataFlags );
	}

private:
	void ApplyLazyStates() {
		// TODO: template magic to factorize the loop kernel.
		if (c_.psSrvDirty_) {
			for ( int i = 0; i < c_.psSrvs_.size(); ++i ) {
				for ( ; i != c_.psSrvs_.size(); ++i ) {
					if ( c_.psSrvDirties_[ i ] )
						break;
				}

				int j = i;
				for ( ; j != c_.psSrvs_.size(); ++j ) {
					if ( !c_.psSrvDirties_[ i ] )
						break;
				}
				if ( i != j ) {
					ctx_->PSSetShaderResources( i, j - i, c_.psSrvs_.data() + i );
				}
				i = j;
			}
			c_.psSrvDirties_.fill(false);
			c_.psSrvDirty_ = false;
		}
		if (c_.samplerStateDirty_) {
			for ( int i = 0; i < c_.samplerStates_.size(); ++i ) {
				for ( ; i != c_.samplerStates_.size(); ++i ) {
					if ( c_.samplerStateDirties_[ i ] )
						break;
				}

				int j = i;
				for ( ; j != c_.samplerStates_.size(); ++j ) {
					if ( !c_.samplerStateDirties_[ i ] )
						break;
				}
				if ( i != j ) {
					ctx_->PSSetSamplers( i, j - i, c_.samplerStates_.data() + i );
				}
				i = j;
			}
			c_.samplerStateDirties_.fill(false);
			c_.samplerStateDirty_ = false;
		}
	}
};

}  // namespace D3D
}  // namespace DX11
