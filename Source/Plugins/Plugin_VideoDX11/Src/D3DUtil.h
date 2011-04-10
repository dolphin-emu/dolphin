// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma	once

#include <d3d11.h>
#include <MathUtil.h>

namespace DX11
{

// simple "smart" pointer which calls AddRef/Release as needed
template <typename T>
class SharedPtr
{
public:
	typedef T* pointer;

	static SharedPtr FromPtr(pointer ptr)
	{
		return SharedPtr(ptr);
	}

	SharedPtr()
		: data(nullptr)
	{}

	SharedPtr(const SharedPtr& other)
		: data(NULL)
	{
		*this = other;
	}

	SharedPtr& operator=(const SharedPtr& other)
	{
		if (other.data)
			other.data->AddRef();

		reset();
		data = other.data;

		return *this;
	}

	~SharedPtr()
	{
		reset();
	}

	void reset()
	{
		if (data)
			data->Release();

		data = nullptr;
	}

	// returning reference for dx functions needing pointer to pointer
	operator pointer const&() const
	{
		return data;
	}

	T& operator*() const
	{
		return *data;
	}

	// overloading operator& for dx functions needing pointer to pointer
	T*const* operator&() const
	{
		return &data;
	}

	pointer operator->() const
	{
		return data;
	}

	bool operator==(const SharedPtr& other) const
	{
		return data == other.data;
	}

	bool operator!=(const SharedPtr& other) const
	{
		return !(*this == other);
	}

private:
	explicit SharedPtr(pointer ptr)
		: data(ptr)
	{}

	pointer data;
};

namespace D3D
{

// Font creation flags
static const u32 D3DFONT_BOLD = 0x0001;
static const u32 D3DFONT_ITALIC = 0x0002;

// Font rendering flags
static const u32 D3DFONT_CENTERED = 0x0001;

class CD3DFont
{
	ID3D11ShaderResourceView* m_pTexture;
	SharedPtr<ID3D11Buffer> m_pVB;
	SharedPtr<ID3D11InputLayout> m_InputLayout;
	SharedPtr<ID3D11PixelShader> m_pshader;
	SharedPtr<ID3D11VertexShader> m_vshader;
	SharedPtr<ID3D11BlendState> m_blendstate;
	ID3D11RasterizerState* m_raststate;
	const int m_dwTexWidth;
	const int m_dwTexHeight;
	unsigned int m_LineHeight;
	float m_fTexCoords[128-32][4];

public:
	CD3DFont();
	// 2D text drawing function
	// Initializing and destroying device-dependent objects
	int Init();
	int Shutdown();
	int DrawTextScaled(float x, float y, float size,
		float spacing, u32 dwColor, const char* strText);
};

extern CD3DFont font;

void InitUtils();
void ShutdownUtils();

void SetPointCopySampler();
void SetLinearCopySampler();

// Draw a quad that fills the viewport using an encoding shader that takes the
// raw pixel position as input
void drawEncoderQuad(SharedPtr<ID3D11PixelShader> pShader,
	ID3D11ClassInstance* const* ppClassInstances = NULL, UINT numClassInstances = 0);

void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
	const D3D11_RECT* rSource,
	int SourceWidth, int SourceHeight,
	ID3D11PixelShader* PShader, ID3D11VertexShader* VShader,
	ID3D11InputLayout* layout, float Gamma = 1.0f);

void drawShadedTexSubQuad(ID3D11ShaderResourceView* texture,
	const MathUtil::Rectangle<float>* rSource,
	int SourceWidth, int SourceHeight,
	const MathUtil::Rectangle<float>* rDest,
	ID3D11PixelShader* PShader, ID3D11VertexShader* Vshader,
	ID3D11InputLayout* layout, float Gamma = 1.0f);

void drawClearQuad(u32 Color, float z, ID3D11PixelShader* PShader, ID3D11VertexShader* Vshader, ID3D11InputLayout* layout);
void drawColorQuad(u32 Color, float x1, float y1, float x2, float y2);

}

}