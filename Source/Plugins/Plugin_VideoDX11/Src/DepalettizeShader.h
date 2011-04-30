// DepalettizeShader.h
// Nolan Check
// Created 4/29/2011

#ifndef _DEPALETTIZESHADER_H
#define _DEPALLETIZESHADER_H

#include "VideoCommon.h"
#include "D3DUtil.h"

namespace DX11
{

class D3DTexture2D;

class DepalettizeShader
{

public:

	enum BaseType
	{
		Uint,
		Unorm4,
		Unorm8
	};

	// Depalettize from baseTex into dstTex using paletteSRV.
	// dstTex should be a render-target with the same dimensions as baseTex.
	// paletteSRV should be a view of an R8G8B8A8_UNORM buffer containing the
	// RGBA values of the palette.
	void Depalettize(D3DTexture2D* dstTex, D3DTexture2D* baseTex,
		BaseType baseType, ID3D11ShaderResourceView* paletteSRV);

private:

	SharedPtr<ID3D11PixelShader> GetShader(BaseType baseType);
	
	// Depalettizing shader for uint indices
	SharedPtr<ID3D11PixelShader> m_uintShader;
	// Depalettizing shader for 4-bit indices as normalized float
	SharedPtr<ID3D11PixelShader> m_unorm4Shader;
	// Depalettizing shader for 8-bit indices as normalized float
	SharedPtr<ID3D11PixelShader> m_unorm8Shader;

};

}

#endif
