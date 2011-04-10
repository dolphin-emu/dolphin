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

#include <list>

#include "D3DBase.h"
#include "D3DUtil.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"
#include "D3DShader.h"
#include "GfxState.h"

namespace DX11
{

namespace D3D
{

// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
	UtilVertexBuffer(UINT size)
		: offset(0), max_size(size)
	{
		D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(max_size, D3D11_BIND_VERTEX_BUFFER,
			D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		m_buf = CreateBufferShared(&desc, NULL);
	}

	// returns vertex offset to the new data
	int AppendData(void* data, UINT size, UINT vertex_size)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		if (offset + size >= max_size)
		{
			// wrap buffer around and notify observers
			offset = 0;
			g_context->Map(m_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

			for each (auto obs in observers)
				*obs = true;
		}
		else
		{
			g_context->Map(m_buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
		}

		offset = ((offset+vertex_size - 1) / vertex_size) * vertex_size; // align offset to vertex_size bytes
		memcpy((u8*)map.pData + offset, data, size);
		g_context->Unmap(m_buf, 0);

		offset += size;
		return (offset - size) / vertex_size;
	}

	void AddWrapObserver(bool* observer)
	{
		observers.push_back(observer);
	}

	ID3D11Buffer*const& GetBuffer() { return m_buf; }

private:
	SharedPtr<ID3D11Buffer> m_buf;
	UINT offset;
	UINT max_size;

	std::list<bool*> observers;
};

CD3DFont font;
std::unique_ptr<UtilVertexBuffer> util_vbuf;

#define MAX_NUM_VERTICES 50*6
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

CD3DFont::CD3DFont()
	: m_dwTexWidth(512), m_dwTexHeight(512)
{
	m_pTexture    = NULL;
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
	// Create vertex buffer for the letters
	HRESULT hr;

	// Prepare to create a bitmap
	unsigned int* pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       =  (int)m_dwTexWidth;
	bmi.bmiHeader.biHeight      = -(int)m_dwTexHeight;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount    = 32;

	// Create a DC and a bitmap for the font
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

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor  (hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_LineHeight = tm.tmHeight;

	// Loop through all printable characters and output them to the bitmap
	// Meanwhile, keep track of the corresponding tex coords for each character.
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
		m_fTexCoords[c][0] = ((float)(x+0))/m_dwTexWidth;
		m_fTexCoords[c][1] = ((float)(y+0))/m_dwTexHeight;
		m_fTexCoords[c][2] = ((float)(x+0+size.cx))/m_dwTexWidth;
		m_fTexCoords[c][3] = ((float)(y+0+size.cy))/m_dwTexHeight;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	//							That way, we can use a static texture
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_dwTexWidth, m_dwTexHeight,
		1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	auto buftex = CreateTexture2DShared(&texdesc, NULL);
	if (!buftex)
	{
		PanicAlert("Failed to create font texture");
		return S_FALSE;
	}
	D3D::SetDebugObjectName(buftex, "texture of a CD3DFont object");

	// Lock the surface and write the alpha values for the set pixels
	D3D11_MAPPED_SUBRESOURCE texmap;
	hr = g_context->Map(buftex, 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
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

	// Done updating texture, so clean up used objects
	g_context->Unmap(buftex, 0);
	hr = D3D::g_device->CreateShaderResourceView(buftex, NULL, &m_pTexture);
	if (FAILED(hr))
		PanicAlert("Failed to create shader resource view at %s %d\n", __FILE__, __LINE__);

	buftex.reset();

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	m_pshader = D3D::CompileAndCreatePixelShader(fontpixshader, sizeof(fontpixshader));
	if (!m_pshader)
		PanicAlert("Failed to create pixel shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(m_pshader, "pixel shader of a CD3DFont object");

	SharedPtr<ID3D10Blob> vsbytecode;
	m_vshader = D3D::CompileAndCreateVertexShader(fontvertshader, sizeof(fontvertshader), std::addressof(vsbytecode));

	if (!m_vshader)
		PanicAlert("Failed to compile/create vertex shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(m_vshader, "vertex shader of a CD3DFont object");

	const D3D11_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	
	m_InputLayout = CreateInputLayoutShared(desc, 3, vsbytecode->GetBufferPointer(),
		vsbytecode->GetBufferSize());
	if (!m_InputLayout)
		PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);

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
	m_blendstate = CreateBlendStateShared(&blenddesc);
	CHECK(m_blendstate, "Create font blend state");
	D3D::SetDebugObjectName(m_blendstate, "blend state of a CD3DFont object");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE,
		false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::g_device->CreateRasterizerState(&rastdesc, &m_raststate);
	CHECK(hr==S_OK, "Create font rasterizer state");
	D3D::SetDebugObjectName(m_raststate, "rasterizer state of a CD3DFont object");

	D3D11_BUFFER_DESC vbdesc = CD3D11_BUFFER_DESC(MAX_NUM_VERTICES * sizeof(FONT2DVERTEX),
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	m_pVB = CreateBufferShared(&vbdesc, NULL);
	if (!m_pVB)
	{
		PanicAlert("Failed to create font vertex buffer at %s, line %d\n", __FILE__, __LINE__);
		return hr;
	}
	D3D::SetDebugObjectName(m_pVB, "vertex buffer of a CD3DFont object");
	return S_OK;
}

int CD3DFont::Shutdown()
{
	m_pVB.reset();
	SAFE_RELEASE(m_pTexture);
	m_InputLayout.reset();
	m_pshader.reset();
	m_vshader.reset();

	m_blendstate.reset();
	SAFE_RELEASE(m_raststate);

	return S_OK;
}

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const char* strText)
{
	if (!m_pVB)
		return 0;

	UINT stride = sizeof(FONT2DVERTEX);
	UINT bufoffset = 0;

	float scalex = 1 / (float)D3D::GetBackBufferWidth() * 2.f;
	float scaley = 1 / (float)D3D::GetBackBufferHeight() * 2.f;
	float sizeratio = size / (float)m_LineHeight;

	// translate starting positions
	float sx = x * scalex - 1.f;
	float sy = 1.f - y * scaley;
	char c;

	// Fill vertex buffer
	FONT2DVERTEX* pVertices;
	int dwNumTriangles = 0L;

	D3D11_MAPPED_SUBRESOURCE vbmap;
	HRESULT hr = g_context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
	if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
	pVertices = (D3D::FONT2DVERTEX*)vbmap.pData;

	// set general pipeline state
	D3D::stateman->PushBlendState(m_blendstate);
	D3D::stateman->PushRasterizerState(m_raststate);
	D3D::stateman->Apply();

	D3D::g_context->PSSetShader(m_pshader, NULL, 0);
	D3D::g_context->VSSetShader(m_vshader, NULL, 0);

	D3D::g_context->IASetInputLayout(m_InputLayout);
	D3D::g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::g_context->PSSetShaderResources(0, 1, &m_pTexture);

	float fStartX = sx;
	while (c = *strText++)
	{
		if (c == ('\n'))
		{
			sx  = fStartX;
			sy -= scaley * size;
		}
		if (c < (' '))
			continue;

		c -= 32;
		float tx1 = m_fTexCoords[c][0];
		float ty1 = m_fTexCoords[c][1];
		float tx2 = m_fTexCoords[c][2];
		float ty2 = m_fTexCoords[c][3];

		float w = (float)(tx2-tx1) * m_dwTexWidth * scalex * sizeratio;
		float h = (float)(ty1-ty2) * m_dwTexHeight * scaley * sizeratio;

		FONT2DVERTEX v[6];
		v[0] = InitFont2DVertex(sx,   sy+h, dwColor, tx1, ty2);
		v[1] = InitFont2DVertex(sx,   sy,   dwColor, tx1, ty1);
		v[2] = InitFont2DVertex(sx+w, sy+h, dwColor, tx2, ty2);
		v[3] = InitFont2DVertex(sx+w, sy,   dwColor, tx2, ty1);
		v[4] = v[2];
		v[5] = v[1];

		memcpy(pVertices, v, 6*sizeof(FONT2DVERTEX));

		pVertices+=6;
		dwNumTriangles += 2;

		if (dwNumTriangles * 3 > (MAX_NUM_VERTICES - 6))
		{
			g_context->Unmap(m_pVB, 0);

			D3D::g_context->IASetVertexBuffers(0, 1, &m_pVB, &stride, &bufoffset);
			D3D::g_context->Draw(3 * dwNumTriangles, 0);

			dwNumTriangles = 0;
			D3D11_MAPPED_SUBRESOURCE vbmap;
			hr = g_context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
			if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
			pVertices = (D3D::FONT2DVERTEX*)vbmap.pData;
		}
		sx += w + spacing * scalex * size;
	}

	// Unlock and render the vertex buffer
	g_context->Unmap(m_pVB, 0);
	if (dwNumTriangles > 0)
	{
		D3D::g_context->IASetVertexBuffers(0, 1, &m_pVB, &stride, &bufoffset);
		D3D::g_context->Draw(3 * dwNumTriangles, 0);
	}
	D3D::stateman->PopBlendState();
	D3D::stateman->PopRasterizerState();
	return S_OK;
}

ID3D11SamplerState* linear_copy_sampler = NULL;
ID3D11SamplerState* point_copy_sampler = NULL;

typedef struct { float x,y,z,u,v,w; } STQVertex;
typedef struct { float x,y,z,u,v,w; } STSQVertex;
typedef struct { float x,y,z; u32 col; } ClearVertex;
typedef struct { float x,y,z; u32 col; } ColVertex;

struct
{
	float u1, v1, u2, v2, G;
} tex_quad_data;

struct
{
	MathUtil::Rectangle<float> rdest;
	float u1, v1, u2, v2, G;
} tex_sub_quad_data;

struct
{
	float x1, y1, x2, y2;
	u32 col;
} draw_quad_data;

struct
{
	u32 col;
	float z;
} clear_quad_data;

// ring buffer offsets
int stq_offset, stsq_offset, cq_offset, clearq_offset;

// observer variables for ring buffer wraps
bool stq_observer, stsq_observer, cq_observer, clearq_observer;

static const D3D11_INPUT_ELEMENT_DESC ENCODER_QUAD_LAYOUT_DESC[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const struct EncoderQuadVertex
{
	float posX;
	float posY;
} ENCODER_QUAD_VERTS[4] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };

static const char ENCODER_VS[] =
"// dolphin-emu generic encoder vertex shader\n"

"float4 main(in float2 Pos : POSITION) : SV_Position\n"
"{\n"
	"return float4(Pos, 0, 1);\n"
"}\n"
;

static SharedPtr<ID3D11Buffer> s_encoderQuad;
static SharedPtr<ID3D11InputLayout> s_encoderQuadLayout;
static SharedPtr<ID3D11VertexShader> s_encoderVShader;

void InitUtils()
{
	util_vbuf.reset(new UtilVertexBuffer(0x4000));

	const float border[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_POINT,
		D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER,
		0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	HRESULT hr = D3D::g_device->CreateSamplerState(&samDesc, &point_copy_sampler);
	if (FAILED(hr))
		PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else
		SetDebugObjectName(point_copy_sampler, "point copy sampler state");

	samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER,
		D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	hr = D3D::g_device->CreateSamplerState(&samDesc, &linear_copy_sampler);
	if (FAILED(hr))
		PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else
		SetDebugObjectName(linear_copy_sampler, "linear copy sampler state");

	// cached data used to avoid unnecessarily reloading the vertex buffers
	memset(&tex_quad_data, 0, sizeof(tex_quad_data));
	memset(&tex_sub_quad_data, 0, sizeof(tex_sub_quad_data));
	memset(&draw_quad_data, 0, sizeof(draw_quad_data));
	memset(&clear_quad_data, 0, sizeof(clear_quad_data));

	// make sure to properly load the vertex data whenever the corresponding functions get called the first time
	stq_observer = stsq_observer = cq_observer = clearq_observer = true;
	util_vbuf->AddWrapObserver(&stq_observer);
	util_vbuf->AddWrapObserver(&stsq_observer);
	util_vbuf->AddWrapObserver(&cq_observer);
	util_vbuf->AddWrapObserver(&clearq_observer);

	font.Init();
	
	// Create resources for encoder quads

	// Create vertex quad
	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(ENCODER_QUAD_VERTS),
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);
	D3D11_SUBRESOURCE_DATA srd = { ENCODER_QUAD_VERTS, 0, 0 };
	s_encoderQuad = CreateBufferShared(&bd, &srd);

	// Create vertex shader
	SharedPtr<ID3D10Blob> bytecode;
	s_encoderVShader = D3D::CompileAndCreateVertexShader(ENCODER_VS, sizeof(ENCODER_VS), std::addressof(bytecode));

	// Create input layout
	s_encoderQuadLayout = CreateInputLayoutShared(ENCODER_QUAD_LAYOUT_DESC,
		sizeof(ENCODER_QUAD_LAYOUT_DESC) / sizeof(D3D11_INPUT_ELEMENT_DESC),
		bytecode->GetBufferPointer(), bytecode->GetBufferSize());
}

void ShutdownUtils()
{
	font.Shutdown();
	SAFE_RELEASE(point_copy_sampler);
	SAFE_RELEASE(linear_copy_sampler);
	util_vbuf.reset();
}

void SetPointCopySampler()
{
	D3D::g_context->PSSetSamplers(0, 1, &point_copy_sampler);
}

void SetLinearCopySampler()
{
	D3D::g_context->PSSetSamplers(0, 1, &linear_copy_sampler);
}

void drawEncoderQuad(SharedPtr<ID3D11PixelShader> pShader,
	ID3D11ClassInstance* const* ppClassInstances, UINT numClassInstances)
{
	// Set up state
	D3D::g_context->PSSetShader(pShader, ppClassInstances, numClassInstances);
	D3D::g_context->VSSetShader(s_encoderVShader, NULL, 0);
	D3D::g_context->IASetInputLayout(s_encoderQuadLayout);
	D3D::g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	UINT stride = sizeof(EncoderQuadVertex);
	UINT offset = 0;
	D3D::g_context->IASetVertexBuffers(0, 1, &s_encoderQuad, &stride, &offset);

	// Encode!
	D3D::g_context->Draw(4, 0);

	// Clean up state
	D3D::g_context->VSSetShader(NULL, NULL, 0);
	D3D::g_context->PSSetShader(NULL, NULL, 0);
}

void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
						const D3D11_RECT* rSource,
						int SourceWidth,
						int SourceHeight,
						ID3D11PixelShader* PShader,
						ID3D11VertexShader* Vshader,
						ID3D11InputLayout* layout,
						float Gamma)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float u1 = ((float)rSource->left) * sw;
	float u2 = ((float)rSource->right) * sw;
	float v1 = ((float)rSource->top) * sh;
	float v2 = ((float)rSource->bottom) * sh;
	float G = 1.0f / Gamma;

	STQVertex coords[4] = {
		{-1.0f, 1.0f, 0.0f,  u1, v1, G},
		{ 1.0f, 1.0f, 0.0f,  u2, v1, G},
		{-1.0f,-1.0f, 0.0f,  u1, v2, G},
		{ 1.0f,-1.0f, 0.0f,  u2, v2, G},
	};

	// only upload the data to VRAM if it changed
	if (stq_observer ||
		tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2 || tex_quad_data.G != G)
	{
		stq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(STQVertex));
		stq_observer = false;

		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
		tex_quad_data.G  =  G;
	}
	UINT stride = sizeof(STQVertex);
	UINT offset = 0;

	D3D::g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::g_context->IASetInputLayout(layout);
	D3D::g_context->IASetVertexBuffers(0, 1, &util_vbuf->GetBuffer(), &stride, &offset);
	D3D::g_context->PSSetShader(PShader, NULL, 0);
	D3D::g_context->PSSetShaderResources(0, 1, &texture);
	D3D::g_context->VSSetShader(Vshader, NULL, 0);
	D3D::stateman->Apply();
	D3D::g_context->Draw(4, stq_offset);

	ID3D11ShaderResourceView* texres = NULL;
	g_context->PSSetShaderResources(0, 1, &texres); // immediately unbind the texture
}

void drawShadedTexSubQuad(ID3D11ShaderResourceView* texture,
							const MathUtil::Rectangle<float>* rSource,
							int SourceWidth,
							int SourceHeight,
							const MathUtil::Rectangle<float>* rDest,
							ID3D11PixelShader* PShader,
							ID3D11VertexShader* Vshader,
							ID3D11InputLayout* layout,
							float Gamma)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float u1 = (rSource->left  ) * sw;
	float u2 = (rSource->right ) * sw;
	float v1 = (rSource->top   ) * sh;
	float v2 = (rSource->bottom) * sh;
	float G = 1.0f / Gamma;

	STSQVertex coords[4] = {
		{ rDest->left , rDest->bottom, 0.0f, u1, v2, G},
		{ rDest->right, rDest->bottom, 0.0f, u2, v2, G},
		{ rDest->left , rDest->top   , 0.0f, u1, v1, G},
		{ rDest->right, rDest->top   , 0.0f, u2, v1, G},
	};

	// only upload the data to VRAM if it changed
	if (stsq_observer ||
		memcmp(rDest, &tex_sub_quad_data.rdest, sizeof(rDest)) != 0 ||
		tex_sub_quad_data.u1 != u1 || tex_sub_quad_data.v1 != v1 ||
		tex_sub_quad_data.u2 != u2 || tex_sub_quad_data.v2 != v2 || tex_sub_quad_data.G != G)
	{
		stsq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(STSQVertex));
		stsq_observer = false;

		tex_sub_quad_data.u1 = u1;
		tex_sub_quad_data.v1 = v1;
		tex_sub_quad_data.u2 = u2;
		tex_sub_quad_data.v2 = v2;
		tex_sub_quad_data.G  = G;
		memcpy(&tex_sub_quad_data.rdest, &rDest, sizeof(rDest));
	}
	UINT stride = sizeof(STSQVertex);
	UINT offset = 0;

	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context->IASetVertexBuffers(0, 1, &util_vbuf->GetBuffer(), &stride, &offset);
	g_context->IASetInputLayout(layout);
	g_context->PSSetShaderResources(0, 1, &texture);
	g_context->PSSetShader(PShader, NULL, 0);
	g_context->VSSetShader(Vshader, NULL, 0);
	stateman->Apply();
	g_context->Draw(4, stsq_offset);

	ID3D11ShaderResourceView* texres = NULL;
	g_context->PSSetShaderResources(0, 1, &texres); // immediately unbind the texture
}

// Fills a certain area of the current render target with the specified color
// destination coordinates normalized to (-1;1)
void drawColorQuad(u32 Color, float x1, float y1, float x2, float y2)
{
	ColVertex coords[4] = {
		{ x1, y2, 0.f, Color },
		{ x2, y2, 0.f, Color },
		{ x1, y1, 0.f, Color },
		{ x2, y1, 0.f, Color },
	};

	if(cq_observer ||
		draw_quad_data.x1 != x1 || draw_quad_data.y1 != y1 ||
		draw_quad_data.x2 != x2 || draw_quad_data.y2 != y2 ||
		draw_quad_data.col != Color)
	{
		cq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ColVertex));
		cq_observer = false;

		draw_quad_data.x1 = x1;
		draw_quad_data.y1 = y1;
		draw_quad_data.x2 = x2;
		draw_quad_data.y2 = y2;
		draw_quad_data.col = Color;
	}

	g_context->VSSetShader(VertexShaderCache::GetClearVertexShader(), NULL, 0);
	g_context->PSSetShader(PixelShaderCache::GetClearProgram(), NULL, 0);
	g_context->IASetInputLayout(VertexShaderCache::GetClearInputLayout());

	UINT stride = sizeof(ColVertex);
	UINT offset = 0;
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context->IASetVertexBuffers(0, 1, &util_vbuf->GetBuffer(), &stride, &offset);

	stateman->Apply();
	g_context->Draw(4, cq_offset);
}

void drawClearQuad(u32 Color, float z, ID3D11PixelShader* PShader, ID3D11VertexShader* Vshader, ID3D11InputLayout* layout)
{
	ClearVertex coords[4] = {
		{-1.0f,  1.0f, z, Color},
		{ 1.0f,  1.0f, z, Color},
		{-1.0f, -1.0f, z, Color},
		{ 1.0f, -1.0f, z, Color},
	};

	if (clearq_observer || clear_quad_data.col != Color || clear_quad_data.z != z)
	{
		clearq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ClearVertex));
		clearq_observer = false;

		clear_quad_data.col = Color;
		clear_quad_data.z = z;
	}
	g_context->VSSetShader(Vshader, NULL, 0);
	g_context->PSSetShader(PShader, NULL, 0);
	g_context->IASetInputLayout(layout);

	UINT stride = sizeof(ClearVertex);
	UINT offset = 0;
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_context->IASetVertexBuffers(0, 1, &util_vbuf->GetBuffer(), &stride, &offset);
	stateman->Apply();
	g_context->Draw(4, clearq_offset);
}

}  // namespace D3D

}  // namespace DX11
