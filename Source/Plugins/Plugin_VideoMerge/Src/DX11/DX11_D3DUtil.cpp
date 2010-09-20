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

// Common
#include "Common.h"

// DX11
#include "DX11_Render.h"
#include "DX11_D3DBase.h"
#include "DX11_D3DUtil.h"
#include "DX11_D3DTexture.h"
#include "DX11_PixelShaderCache.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_D3DShader.h"

namespace DX11
{

namespace D3D
{

CD3DFont font;

#define MAX_NUM_VERTICES 300
struct FONT2DVERTEX {
	float x,y,z;
	float col[4];
	float tu, tv;
};

inline FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x=x; v.y=y; v.z=0;  v.tu = tu; v.tv = tv;
	v.col[0] = ((float)((color >> 16) & 0xFF)) / 255.f;
	v.col[1] = ((float)((color >>  8) & 0xFF)) / 255.f;
	v.col[2] = ((float)((color >>  0) & 0xFF)) / 255.f;
	v.col[3] = ((float)((color >> 24) & 0xFF)) / 255.f;
	return v;
}

CD3DFont::CD3DFont() : m_dwTexWidth(512), m_dwTexHeight(512)
{
	m_pTexture    = NULL;
	m_pVB         = NULL;
	m_InputLayout = NULL;
	m_pshader     = NULL;
	m_vshader     = NULL;
}

const char fontpixshader[] = {
	"Texture2D tex2D;\n"
	"SamplerState linearSampler\n"
	"{\n"
	"	Filter = MIN_MAG_MIP_LINEAR;\n"
	"	AddressU = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	AddressV = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	BorderColor = float4(0.f, 0.f, 0.f, 0.f);\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"float4 main( PS_INPUT input ) : SV_Target\n"
	"{\n"
	"	return tex2D.Sample( linearSampler, input.tex ) * input.col;\n"
	"};\n"
};

const char fontvertshader[] = {
	"struct VS_INPUT\n"
	"{\n"
	"	float4 pos : POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"PS_INPUT main( VS_INPUT input )\n"
	"{\n"
	"	PS_INPUT output;\n"
	"	output.pos = input.pos;\n"
	"	output.col = input.col;\n"
	"	output.tex = input.tex;\n"
	"	return output;\n"
	"};\n"
};

int CD3DFont::Init()
{
	HRESULT hr;

	// prepare to create a bitmap
	unsigned int* pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       =  (int)m_dwTexWidth;
	bmi.bmiHeader.biHeight      = -(int)m_dwTexHeight;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount    = 32;

	// create a DC and a bitmap for the font
	HDC hDC = CreateCompatibleDC(NULL);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, NULL, 0);
	SetMapMode(hDC, MM_TEXT);

	// create a GDI font
	HFONT hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE,
							FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
							VARIABLE_PITCH, _T("Tahoma"));
	if (NULL == hFont) return E_FAIL;

	HGDIOBJ hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	// set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor  (hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_LineHeight = tm.tmHeight;

	// loop through all printable characters and output them to the bitmap
	// meanwhile, keep track of the corresponding tex coords for each character.
	int x = 0, y = 0;
	char str[2] = "\0";
	for (int c = 0; c < 127 - 32; c++)
	{
		str[0] = c + 32;
		SIZE size;
		GetTextExtentPoint32A(hDC, str, 1, &size);
		if ((int)(x+size.cx+1) > m_dwTexWidth)
		{
			x  = 0;
			y += m_LineHeight;
		}

		ExtTextOutA(hDC, x+1, y+0, ETO_OPAQUE | ETO_CLIPPED, NULL, str, 1, NULL);
		m_fTexCoords[c][0] = (float) x         /m_dwTexWidth;
		m_fTexCoords[c][1] = (float) y         /m_dwTexHeight;
		m_fTexCoords[c][2] = (float)(x+size.cx)/m_dwTexWidth;
		m_fTexCoords[c][3] = (float)(y+size.cy)/m_dwTexHeight;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	//							That way, we can use a static texture
	ID3D11Texture2D* buftex;
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_dwTexWidth, m_dwTexHeight,
										1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC,
										D3D11_CPU_ACCESS_WRITE);
	hr = device->CreateTexture2D(&texdesc, NULL, &buftex);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create font texture");
		return hr;
	}
	D3D::SetDebugObjectName((ID3D11DeviceChild*)buftex, "texture of a CD3DFont object");

	// lock the surface and write the alpha values for the set pixels
	D3D11_MAPPED_SUBRESOURCE texmap;
	hr = context->Map(buftex, 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
	if (FAILED(hr)) PanicAlert("Failed to map a texture at %s %d\n", __FILE__, __LINE__);

	for (y = 0; y < m_dwTexHeight; y++)
	{
		u32* pDst32 = (u32*)((u8*)texmap.pData + y * texmap.RowPitch);
		for (x = 0; x < m_dwTexWidth; x++)
		{
			const u8 bAlpha = (pBitmapBits[m_dwTexWidth * y + x] & 0xff);
			*pDst32++ = (((bAlpha << 4) | bAlpha) << 24) | 0xFFFFFF;
		}
	}

	// clean up
	context->Unmap(buftex, 0);
	hr = D3D::device->CreateShaderResourceView(buftex, NULL, &m_pTexture);
	if (FAILED(hr)) PanicAlert("Failed to create shader resource view at %s %d\n", __FILE__, __LINE__);
	SAFE_RELEASE(buftex);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	m_pshader = D3D::CompileAndCreatePixelShader(fontpixshader, sizeof(fontpixshader));
	if (m_pshader == NULL) PanicAlert("Failed to create pixel shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_pshader, "pixel shader of a CD3DFont object");

	D3DBlob* vsbytecode;
	D3D::CompileVertexShader(fontvertshader, sizeof(fontvertshader), &vsbytecode);
	if (vsbytecode == NULL) PanicAlert("Failed to compile vertex shader, %s %d\n", __FILE__, __LINE__);
	m_vshader = D3D::CreateVertexShaderFromByteCode(vsbytecode);
	if (m_vshader == NULL) PanicAlert("Failed to create vertex shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_vshader, "vertex shader of a CD3DFont object");

	const D3D11_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = D3D::device->CreateInputLayout(desc, 3, vsbytecode->Data(), vsbytecode->Size(), &m_InputLayout);
	if (FAILED(hr)) PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);
	SAFE_RELEASE(vsbytecode);

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = TRUE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = D3D::device->CreateBlendState(&blenddesc, &m_blendstate);
	CHECK(hr==S_OK, "Create font blend state");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_blendstate, "blend state of a CD3DFont object");

	// this might need to be changed when adding multisampling support
	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, &m_raststate);
	CHECK(hr==S_OK, "Create font rasterizer state");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_raststate, "rasterizer state of a CD3DFont object");

	D3D11_BUFFER_DESC vbdesc = CD3D11_BUFFER_DESC(MAX_NUM_VERTICES*sizeof(FONT2DVERTEX), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	if (FAILED(hr = device->CreateBuffer(&vbdesc, NULL, &m_pVB)))
	{
		PanicAlert("Failed to create font vertex buffer at %s, line %d\n", __FILE__, __LINE__);
		return hr;
	}
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_pVB, "vertex buffer of a CD3DFont object");
	return S_OK;
}

int CD3DFont::Shutdown()
{
	SAFE_RELEASE(m_pVB);
	SAFE_RELEASE(m_pTexture);
	SAFE_RELEASE(m_InputLayout);
	SAFE_RELEASE(m_pshader);
	SAFE_RELEASE(m_vshader);

	SAFE_RELEASE(m_blendstate);
	SAFE_RELEASE(m_raststate);

	return S_OK;
}

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const char* strText, bool center)
{
	if (!m_pVB) return 0;

	UINT stride = sizeof(FONT2DVERTEX);
	UINT bufoffset = 0;

	float scalex = 1 / (float)D3D::GetBackBufferWidth() * 2.f;
	float scaley = 1 / (float)D3D::GetBackBufferHeight() * 2.f;
	float sizeratio = size / (float)m_LineHeight;

	// translate starting positions
	float sx = x * scalex - 1.f;
	float sy = 1.f - y * scaley;
	char c;

	// fill vertex buffer
	FONT2DVERTEX* pVertices;
	int dwNumTriangles = 0L;

	D3D11_MAPPED_SUBRESOURCE vbmap;
	HRESULT hr = context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
	if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
	pVertices = (D3D::FONT2DVERTEX*)vbmap.pData;

	// if center was requested, set current position as centre
	// this is currently never used
	if (center)
	{
		const char *oldText = strText;
		float mx=0;
		float maxx=0;

		while (c = *strText++)
		{
			if (c == ('\n')) mx = 0;
			if (c <  (' ') ) continue;
			c -= 32;
			mx += (m_fTexCoords[c][2]-m_fTexCoords[c][0])/(m_fTexCoords[0][3] - m_fTexCoords[0][1]) + spacing;
			if (mx > maxx) maxx = mx;
		}
		sx -= scalex*maxx*size;
		strText = oldText;
	}
	// set general pipeline state
	D3D::stateman->PushBlendState(m_blendstate);
	D3D::stateman->PushRasterizerState(m_raststate);
	D3D::stateman->Apply();

	D3D::context->PSSetShader(m_pshader, NULL, 0);
	D3D::context->VSSetShader(m_vshader, NULL, 0);

	D3D::context->IASetInputLayout(m_InputLayout);
	D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::context->PSSetShaderResources(0, 1, &m_pTexture);

	float fStartX = sx;
	while (c = *strText++)
	{
		if (c == ('\n'))
		{
			sx  = fStartX;
			sy -= scaley * size;
		}
		if (c < (' ')) continue;

		c -= 32;
		float tx1 = m_fTexCoords[c][0];
		float ty1 = m_fTexCoords[c][1];
		float tx2 = m_fTexCoords[c][2];
		float ty2 = m_fTexCoords[c][3];

		float w = (float)(tx2-tx1) * m_dwTexWidth * scalex * sizeratio;
		float h = (float)(ty1-ty2) * m_dwTexHeight * scaley * sizeratio;

		FONT2DVERTEX v[6];
		v[0] = InitFont2DVertex(  sx, h+sy, dwColor, tx1, ty2);
		v[1] = InitFont2DVertex(  sx,   sy, dwColor, tx1, ty1);
		v[2] = InitFont2DVertex(w+sx, h+sy, dwColor, tx2, ty2);
		v[3] = InitFont2DVertex(w+sx,   sy, dwColor, tx2, ty1);
		v[4] = v[2];
		v[5] = v[1];

		memcpy(pVertices, v, 6*sizeof(FONT2DVERTEX));

		pVertices+=6;
		dwNumTriangles += 2;

		if (dwNumTriangles * 3 > (MAX_NUM_VERTICES - 6))
		{
			context->Unmap(m_pVB, 0);

			D3D::context->IASetVertexBuffers(0, 1, &m_pVB, &stride, &bufoffset);
			D3D::context->Draw(3 * dwNumTriangles, 0);

			dwNumTriangles = 0;
			D3D11_MAPPED_SUBRESOURCE vbmap;
			hr = context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
			if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
			pVertices = (D3D::FONT2DVERTEX*)vbmap.pData;
		}
		sx += w + spacing * scalex * size;
	}

	// Unlock and render the vertex buffer
	context->Unmap(m_pVB, 0);
	if (dwNumTriangles > 0)
	{
		D3D::context->IASetVertexBuffers(0, 1, &m_pVB, &stride, &bufoffset);
		D3D::context->Draw(3 * dwNumTriangles, 0);
	}
	D3D::stateman->PopBlendState();
	D3D::stateman->PopRasterizerState();
	return S_OK;
}

ID3D11Buffer* CreateQuadVertexBuffer(unsigned int size, void* data)
{
	ID3D11Buffer* vb;
	D3D11_BUFFER_DESC vbdesc;
	vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbdesc.ByteWidth = size;
	vbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbdesc.MiscFlags = 0;
	vbdesc.Usage = D3D11_USAGE_DYNAMIC;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA bufdata;
		bufdata.pSysMem = data;
		if (FAILED(device->CreateBuffer(&vbdesc, &bufdata, &vb))) return NULL;
	}
	else if (FAILED(device->CreateBuffer(&vbdesc, NULL, &vb))) return NULL;

	return vb;
}

ID3D11SamplerState* linear_copy_sampler = NULL;
ID3D11SamplerState* point_copy_sampler = NULL;
ID3D11Buffer* stqvb = NULL;
ID3D11Buffer* stsqvb = NULL;
ID3D11Buffer* clearvb = NULL;

typedef struct { float x,y,z,u,v; } STQVertex;
typedef struct { float x,y,z,u,v; } STSQVertex;
typedef struct { float x,y,z; u32 col; } ClearVertex;

struct
{
	float u1, v1, u2, v2;
} tex_quad_data;

struct
{
	MathUtil::Rectangle<float> rdest;
	float u1, v1, u2, v2;
} tex_sub_quad_data;

struct
{
	u32 col;
	float z;
} clear_quad_data;

void InitUtils()
{
	float border[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, -D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX);
	HRESULT hr = D3D::device->CreateSamplerState(&samDesc, &point_copy_sampler);
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName((ID3D11DeviceChild*)point_copy_sampler, "point copy sampler state");

	samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, -D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX);
	hr = D3D::device->CreateSamplerState(&samDesc, &linear_copy_sampler);
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName((ID3D11DeviceChild*)linear_copy_sampler, "linear copy sampler state");

	// cached data used to avoid unnecessarily reloading the vertex buffers
	memset(&tex_quad_data, 0, sizeof(tex_quad_data));
	memset(&tex_sub_quad_data, 0, sizeof(tex_sub_quad_data));
	memset(&clear_quad_data, 0, sizeof(clear_quad_data));

	STQVertex stqcoords[4] = {
		{-1.0f, 1.0f, 0.0f, 0, 0},
		{ 1.0f, 1.0f, 0.0f, 0, 0},
		{-1.0f,-1.0f, 0.0f, 0, 0},
		{ 1.0f,-1.0f, 0.0f, 0, 0},
	};

	STSQVertex stsqcoords[4];
	memset(stsqcoords, 0, sizeof(stsqcoords));

	ClearVertex cqcoords[4] = {
		{-1.0f,  1.0f, 0, 0},
		{ 1.0f,  1.0f, 0, 0},
		{-1.0f, -1.0f, 0, 0},
		{ 1.0f, -1.0f, 0, 0},
	};

	stqvb = CreateQuadVertexBuffer(4*sizeof(STQVertex), stqcoords);
	CHECK(stqvb!=NULL, "Create vertex buffer of drawShadedTexQuad");
	SetDebugObjectName((ID3D11DeviceChild*)stqvb, "vertex buffer of drawShadedTexQuad");

	stsqvb = CreateQuadVertexBuffer(4*sizeof(STSQVertex), stsqcoords);
	CHECK(stsqvb!=NULL, "Create vertex buffer of drawShadedTexSubQuad");
	SetDebugObjectName((ID3D11DeviceChild*)stsqvb, "vertex buffer of drawShadedTexSubQuad");

	clearvb = CreateQuadVertexBuffer(4*sizeof(ClearVertex), cqcoords);
	CHECK(clearvb!=NULL, "Create vertex buffer of drawClearQuad");
	SetDebugObjectName((ID3D11DeviceChild*)clearvb, "vertex buffer of drawClearQuad");

	font.Init();
}

void ShutdownUtils()
{
	font.Shutdown();
	SAFE_RELEASE(point_copy_sampler);
	SAFE_RELEASE(linear_copy_sampler);
	SAFE_RELEASE(stqvb);
	SAFE_RELEASE(stsqvb);
	SAFE_RELEASE(clearvb);
}

void SetPointCopySampler()
{
	D3D::context->PSSetSamplers(0, 1, &point_copy_sampler);
}

void SetLinearCopySampler()
{
	D3D::context->PSSetSamplers(0, 1, &linear_copy_sampler);
}

void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
						const D3D11_RECT* rSource,
						int SourceWidth,
						int SourceHeight,
						ID3D11PixelShader* PShader,
						ID3D11VertexShader* Vshader,
						ID3D11InputLayout* layout)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float u1 = ((float)rSource->left) * sw;
	float u2 = ((float)rSource->right) * sw;
	float v1 = ((float)rSource->top) * sh;
	float v2 = ((float)rSource->bottom) * sh;

	STQVertex coords[4] = {
		{-1.0f, 1.0f, 0.0f,  u1, v1},
		{ 1.0f, 1.0f, 0.0f,  u2, v1},
		{-1.0f,-1.0f, 0.0f,  u1, v2},
		{ 1.0f,-1.0f, 0.0f,  u2, v2},
	};

	// only upload the data to VRAM if it changed
	if (tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(stqvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, coords, sizeof(coords));
		D3D::context->Unmap(stqvb, 0);
		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
	}
	UINT stride = sizeof(STQVertex);
	UINT offset = 0;

	D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::context->IASetInputLayout(layout);
	D3D::context->IASetVertexBuffers(0, 1, &stqvb, &stride, &offset);
	D3D::context->PSSetShader(PShader, NULL, 0);
	D3D::context->PSSetShaderResources(0, 1, &texture);
	D3D::context->VSSetShader(Vshader, NULL, 0);
	D3D::stateman->Apply();
	D3D::context->Draw(4, 0);

	ID3D11ShaderResourceView* texres = NULL;
	context->PSSetShaderResources(0, 1, &texres); // immediately unbind the texture
}

void drawShadedTexSubQuad(ID3D11ShaderResourceView* texture,
							const MathUtil::Rectangle<float>* rSource,
							int SourceWidth,
							int SourceHeight,
							const MathUtil::Rectangle<float>* rDest,
							ID3D11PixelShader* PShader,
							ID3D11VertexShader* Vshader,
							ID3D11InputLayout* layout)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float u1 = (rSource->left  ) * sw;
	float u2 = (rSource->right ) * sw;
	float v1 = (rSource->top   ) * sh;
	float v2 = (rSource->bottom) * sh;

	STSQVertex coords[4] = {
		{ rDest->left , rDest->bottom, 0.0f, u1, v1},
		{ rDest->right, rDest->bottom, 0.0f, u2, v1},
		{ rDest->left , rDest->top   , 0.0f, u1, v2},
		{ rDest->right, rDest->top   , 0.0f, u2, v2},
	};

	// only upload the data to VRAM if it changed
	if (memcmp(rDest, &tex_sub_quad_data.rdest, sizeof(rDest)) != 0 ||
		tex_sub_quad_data.u1 != u1 || tex_sub_quad_data.v1 != v1 ||
		tex_sub_quad_data.u2 != u2 || tex_sub_quad_data.v2 != v2)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(stsqvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, coords, sizeof(coords));
		D3D::context->Unmap(stsqvb, 0);
		tex_sub_quad_data.u1 = u1;
		tex_sub_quad_data.v1 = v1;
		tex_sub_quad_data.u2 = u2;
		tex_sub_quad_data.v2 = v2;
		memcpy(&tex_sub_quad_data.rdest, &rDest, sizeof(rDest));
	}
	UINT stride = sizeof(STSQVertex);
	UINT offset = 0;

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->IASetVertexBuffers(0, 1, &stsqvb, &stride, &offset);
	context->IASetInputLayout(layout);
	context->PSSetShaderResources(0, 1, &texture);
	context->PSSetShader(PShader, NULL, 0);	
	context->VSSetShader(Vshader, NULL, 0);	
	stateman->Apply();
	context->Draw(4, 0);

	ID3D11ShaderResourceView* texres = NULL;
	context->PSSetShaderResources(0, 1, &texres); // immediately unbind the texture
}

void drawClearQuad(u32 Color, float z, ID3D11PixelShader* PShader, ID3D11VertexShader* Vshader, ID3D11InputLayout* layout)
{
	if (clear_quad_data.col != Color || clear_quad_data.z != z)
	{
		const ClearVertex coords[4] = {
			{-1.0f,  1.0f, z, Color},
			{ 1.0f,  1.0f, z, Color},
			{-1.0f, -1.0f, z, Color},
			{ 1.0f, -1.0f, z, Color},
		};

		D3D11_MAPPED_SUBRESOURCE map;
		context->Map(clearvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, coords, sizeof(coords));
		context->Unmap(clearvb, 0);
		clear_quad_data.col = Color;
		clear_quad_data.z = z;
	}
	context->VSSetShader(Vshader, NULL, 0);
	context->PSSetShader(PShader, NULL, 0);
	context->IASetInputLayout(layout);

	UINT stride = sizeof(ClearVertex);
	UINT offset = 0;
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->IASetVertexBuffers(0, 1, &clearvb, &stride, &offset);
	stateman->Apply();
	context->Draw(4, 0);
}


}  // namespace

}
