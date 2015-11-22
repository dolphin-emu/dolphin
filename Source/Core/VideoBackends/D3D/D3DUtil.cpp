// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cctype>
#include <list>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

namespace DX11
{

namespace D3D
{

// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
	explicit UtilVertexBuffer(int size) : buf(nullptr), offset(0), max_size(size)
	{
		D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(max_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		device->CreateBuffer(&desc, nullptr, &buf);
	}
	~UtilVertexBuffer()
	{
		buf->Release();
	}

	// returns vertex offset to the new data
	int AppendData(void* data, int size, int vertex_size)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		if (offset + size >= max_size)
		{
			// wrap buffer around and notify observers
			offset = 0;
			context->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

			for (auto observer : observers)
				*observer = true;
		}
		else
		{
			context->Map(buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
		}
		offset = (offset+vertex_size-1)/vertex_size*vertex_size; // align offset to vertex_size bytes
		memcpy(static_cast<u8*>(map.pData) + offset, data, size);
		context->Unmap(buf, 0);

		offset += size;
		return (offset - size) / vertex_size;
	}

	void AddWrapObserver(bool* observer)
	{
		observers.push_back(observer);
	}

	ID3D11Buffer* &GetBuffer() { return buf; }

private:
	ID3D11Buffer* buf;
	int offset;
	int max_size;

	std::list<bool*> observers;
};

CD3DFont font;
UtilVertexBuffer* util_vbuf = nullptr;

#define MAX_NUM_VERTICES 50*6
struct FONT2DVERTEX {
	float x,y,z;
	float col[4];
	float tu, tv;
};

inline FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x=x; v.y=y; v.z=0;  v.tu = tu; v.tv = tv;
	v.col[0] = static_cast<float>(color >> 16 & 0xFF) / 255.f;
	v.col[1] = static_cast<float>(color >>  8 & 0xFF) / 255.f;
	v.col[2] = static_cast<float>(color >>  0 & 0xFF) / 255.f;
	v.col[3] = static_cast<float>(color >> 24 & 0xFF) / 255.f;
	return v;
}

CD3DFont::CD3DFont() : m_dwTexWidth(512), m_dwTexHeight(512)
{
	m_pTexture    = nullptr;
	m_pVB         = nullptr;
	m_InputLayout = nullptr;
	m_pshader     = nullptr;
	m_vshader     = nullptr;
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
	bmi.bmiHeader.biWidth       =  static_cast<int>(m_dwTexWidth);
	bmi.bmiHeader.biHeight      = -static_cast<int>(m_dwTexHeight);
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount    = 32;

	// Create a DC and a bitmap for the font
	auto hDC = CreateCompatibleDC(nullptr);
	auto hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBitmapBits), nullptr, 0);
	SetMapMode(hDC, MM_TEXT);

	// create a GDI font
	auto hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE,
							FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
							VARIABLE_PITCH, _T("Tahoma"));
	if (nullptr == hFont) return E_FAIL;

	auto hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	auto hOldFont = SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor  (hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_LineHeight = tm.tmHeight;

	// Loop through all printable characters and output them to the bitmap
	// Meanwhile, keep track of the corresponding tex coords for each character.
	auto x = 0, y = 0;
	char str[2] = "\0";
	for (auto c = 0; c < 127 - 32; c++)
	{
		str[0] = c + 32;
		SIZE size;
		GetTextExtentPoint32A(hDC, str, 1, &size);
		if (static_cast<int>(x+size.cx+1) > m_dwTexWidth)
		{
			x  = 0;
			y += m_LineHeight;
		}

		ExtTextOutA(hDC, x+1, y+0, ETO_OPAQUE | ETO_CLIPPED, nullptr, str, 1, nullptr);
		m_fTexCoords[c][0] = static_cast<float>(x+0)/m_dwTexWidth;
		m_fTexCoords[c][1] = static_cast<float>(y+0)/m_dwTexHeight;
		m_fTexCoords[c][2] = static_cast<float>(x+0+size.cx)/m_dwTexWidth;
		m_fTexCoords[c][3] = static_cast<float>(y+0+size.cy)/m_dwTexHeight;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	// That way, we can use a static texture
	ID3D11Texture2D* buftex;
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_dwTexWidth, m_dwTexHeight,
										1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC,
										D3D11_CPU_ACCESS_WRITE);
	hr = device->CreateTexture2D(&texdesc, nullptr, &buftex);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create font texture");
		return hr;
	}
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(buftex), "texture of a CD3DFont object");

	// Lock the surface and write the alpha values for the set pixels
	D3D11_MAPPED_SUBRESOURCE texmap;
	hr = context->Map(buftex, 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
	if (FAILED(hr)) PanicAlert("Failed to map a texture at %s %d\n", __FILE__, __LINE__);

	for (y = 0; y < m_dwTexHeight; y++)
	{
		auto pDst32 = reinterpret_cast<u32*>(static_cast<u8*>(texmap.pData) + y * texmap.RowPitch);
		for (x = 0; x < m_dwTexWidth; x++)
		{
			const u8 bAlpha = (pBitmapBits[m_dwTexWidth * y + x] & 0xff);
			*pDst32++ = (((bAlpha << 4) | bAlpha) << 24) | 0xFFFFFF;
		}
	}

	// Done updating texture, so clean up used objects
	context->Unmap(buftex, 0);
	hr = device->CreateShaderResourceView(buftex, nullptr, &m_pTexture);
	if (FAILED(hr)) PanicAlert("Failed to create shader resource view at %s %d\n", __FILE__, __LINE__);
	SAFE_RELEASE(buftex);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	m_pshader = CompileAndCreatePixelShader(fontpixshader);
	if (m_pshader == nullptr) PanicAlert("Failed to create pixel shader, %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(m_pshader), "pixel shader of a CD3DFont object");

	D3DBlob* vsbytecode;
	CompileVertexShader(fontvertshader, &vsbytecode);
	if (vsbytecode == nullptr) PanicAlert("Failed to compile vertex shader, %s %d\n", __FILE__, __LINE__);
	m_vshader = CreateVertexShaderFromByteCode(vsbytecode);
	if (m_vshader == nullptr) PanicAlert("Failed to create vertex shader, %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(m_vshader), "vertex shader of a CD3DFont object");

	const D3D11_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = device->CreateInputLayout(desc, 3, vsbytecode->Data(), vsbytecode->Size(), &m_InputLayout);
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
	hr = device->CreateBlendState(&blenddesc, &m_blendstate);
	CHECK(hr==S_OK, "Create font blend state");
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(m_blendstate), "blend state of a CD3DFont object");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = device->CreateRasterizerState(&rastdesc, &m_raststate);
	CHECK(hr==S_OK, "Create font rasterizer state");
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(m_raststate), "rasterizer state of a CD3DFont object");

	D3D11_BUFFER_DESC vbdesc = CD3D11_BUFFER_DESC(MAX_NUM_VERTICES*sizeof(FONT2DVERTEX), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	if (FAILED(hr = device->CreateBuffer(&vbdesc, nullptr, &m_pVB)))
	{
		PanicAlert("Failed to create font vertex buffer at %s, line %d\n", __FILE__, __LINE__);
		return hr;
	}
	SetDebugObjectName(static_cast<ID3D11DeviceChild*>(m_pVB), "vertex buffer of a CD3DFont object");
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

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const std::string& text)
{
	if (!m_pVB)
		return 0;

	UINT stride = sizeof(FONT2DVERTEX);
	UINT bufoffset = 0;

	auto scalex = 1 / static_cast<float>(GetBackBufferWidth()) * 2.f;
	auto scaley = 1 / static_cast<float>(GetBackBufferHeight()) * 2.f;
	auto sizeratio = size / static_cast<float>(m_LineHeight);

	// translate starting positions
	auto sx = x * scalex - 1.f;
	auto sy = 1.f - y * scaley;

	// Fill vertex buffer
	FONT2DVERTEX* pVertices;
	int dwNumTriangles = 0L;

	D3D11_MAPPED_SUBRESOURCE vbmap;
	auto hr = context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
	if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
	pVertices = static_cast<FONT2DVERTEX*>(vbmap.pData);

	// set general pipeline state
	stateman->PushBlendState(m_blendstate);
	stateman->PushRasterizerState(m_raststate);

	stateman->SetPixelShader(m_pshader);
	stateman->SetVertexShader(m_vshader);
	stateman->SetGeometryShader(nullptr);

	stateman->SetInputLayout(m_InputLayout);
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	stateman->SetTexture(0, m_pTexture);

	auto fStartX = sx;
	for (auto c : text)
	{
		if (c == '\n')
		{
			sx  = fStartX;
			sy -= scaley * size;
		}
		if (!isprint(c))
			continue;

		c -= 32;
		auto tx1 = m_fTexCoords[c][0];
		auto ty1 = m_fTexCoords[c][1];
		auto tx2 = m_fTexCoords[c][2];
		auto ty2 = m_fTexCoords[c][3];

		auto w = static_cast<float>(tx2-tx1) * m_dwTexWidth * scalex * sizeratio;
		auto h = static_cast<float>(ty1-ty2) * m_dwTexHeight * scaley * sizeratio;

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
			context->Unmap(m_pVB, 0);

			stateman->SetVertexBuffer(m_pVB, stride, bufoffset);

			stateman->Apply();
			context->Draw(3 * dwNumTriangles, 0);

			dwNumTriangles = 0;
			D3D11_MAPPED_SUBRESOURCE _vbmap;
			hr = context->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &_vbmap);
			if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
			pVertices = static_cast<FONT2DVERTEX*>(_vbmap.pData);
		}
		sx += w + spacing * scalex * size;
	}

	// Unlock and render the vertex buffer
	context->Unmap(m_pVB, 0);
	if (dwNumTriangles > 0)
	{
		stateman->SetVertexBuffer(m_pVB, stride, bufoffset);

		stateman->Apply();
		context->Draw(3 * dwNumTriangles, 0);
	}
	stateman->PopBlendState();
	stateman->PopRasterizerState();
	return S_OK;
}

ID3D11SamplerState* linear_copy_sampler = nullptr;
ID3D11SamplerState* point_copy_sampler = nullptr;

struct STQVertex   { float x, y, z, u, v, w, g; };
struct STSQVertex  { float x, y, z, u, v, w, g; };
struct ClearVertex { float x, y, z; u32 col; };
struct ColVertex   { float x, y, z; u32 col; };

struct
{
	float u1, v1, u2, v2, S, G;
} tex_quad_data;

struct
{
	MathUtil::Rectangle<float> rdest;
	float u1, v1, u2, v2, S, G;
} tex_sub_quad_data;

struct
{
	float x1, y1, x2, y2, z;
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

void InitUtils()
{
	util_vbuf = new UtilVertexBuffer(0x4000);

	float border[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	auto hr = device->CreateSamplerState(&samDesc, &point_copy_sampler);
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName(static_cast<ID3D11DeviceChild*>(point_copy_sampler), "point copy sampler state");

	samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	hr = device->CreateSamplerState(&samDesc, &linear_copy_sampler);
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName(static_cast<ID3D11DeviceChild*>(linear_copy_sampler), "linear copy sampler state");

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
}

void ShutdownUtils()
{
	font.Shutdown();
	SAFE_RELEASE(point_copy_sampler);
	SAFE_RELEASE(linear_copy_sampler);
	SAFE_DELETE(util_vbuf);
}

void SetPointCopySampler()
{
	stateman->SetSampler(0, point_copy_sampler);
}

void SetLinearCopySampler()
{
	stateman->SetSampler(0, linear_copy_sampler);
}

void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
						const D3D11_RECT* rSource,
						int SourceWidth,
						int SourceHeight,
						ID3D11PixelShader* PShader,
						ID3D11VertexShader* VShader,
						ID3D11InputLayout* layout,
						ID3D11GeometryShader* GShader,
						float Gamma,
						u32 slice)
{
	auto sw = 1.0f /static_cast<float>(SourceWidth);
	auto sh = 1.0f /static_cast<float>(SourceHeight);
	auto u1 = static_cast<float>(rSource->left) * sw;
	auto u2 = static_cast<float>(rSource->right) * sw;
	auto v1 = static_cast<float>(rSource->top) * sh;
	auto v2 = static_cast<float>(rSource->bottom) * sh;
	auto S = static_cast<float>(slice);
	auto G = 1.0f / Gamma;

	STQVertex coords[4] = {
		{-1.0f, 1.0f, 0.0f,  u1, v1, S, G},
		{ 1.0f, 1.0f, 0.0f,  u2, v1, S, G},
		{-1.0f,-1.0f, 0.0f,  u1, v2, S, G},
		{ 1.0f,-1.0f, 0.0f,  u2, v2, S, G},
	};

	// only upload the data to VRAM if it changed
	if (stq_observer ||
		tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2 ||
		tex_quad_data.S  != S  || tex_quad_data.G  != G)
	{
		stq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(STQVertex));
		stq_observer = false;

		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
		tex_quad_data.S  = S;
		tex_quad_data.G  = G;
	}
	UINT stride = sizeof(STQVertex);
	UINT offset = 0;

	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	stateman->SetInputLayout(layout);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);
	stateman->SetPixelShader(PShader);
	stateman->SetTexture(0, texture);
	stateman->SetVertexShader(VShader);
	stateman->SetGeometryShader(GShader);

	stateman->Apply();
	context->Draw(4, stq_offset);

	stateman->SetTexture(0, nullptr); // immediately unbind the texture
	stateman->Apply();

	stateman->SetGeometryShader(nullptr);
}

// Fills a certain area of the current render target with the specified color
// destination coordinates normalized to (-1;1)
void drawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2)
{
	ColVertex coords[4] = {
		{ x1, y1, z, Color },
		{ x2, y1, z, Color },
		{ x1, y2, z, Color },
		{ x2, y2, z, Color },
	};

	if (cq_observer ||
	    draw_quad_data.x1 != x1 || draw_quad_data.y1 != y1 ||
	    draw_quad_data.x2 != x2 || draw_quad_data.y2 != y2 ||
	    draw_quad_data.col != Color || draw_quad_data.z != z)
	{
		cq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ColVertex));
		cq_observer = false;

		draw_quad_data.x1 = x1;
		draw_quad_data.y1 = y1;
		draw_quad_data.x2 = x2;
		draw_quad_data.y2 = y2;
		draw_quad_data.col = Color;
		draw_quad_data.z = z;
	}

	stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
	stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
	stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

	UINT stride = sizeof(ColVertex);
	UINT offset = 0;
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

	stateman->Apply();
	context->Draw(4, cq_offset);

	stateman->SetGeometryShader(nullptr);
}

void drawClearQuad(u32 Color, float z)
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

	stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
	stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
	stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

	UINT stride = sizeof(ClearVertex);
	UINT offset = 0;
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

	stateman->Apply();
	context->Draw(4, clearq_offset);

	stateman->SetGeometryShader(nullptr);
}

}  // namespace D3D

}  // namespace DX11
