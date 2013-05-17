// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "StringUtil.h"

#include "D3DBase.h"
#include "D3DUtil.h"
#include "Render.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"

namespace DX9
{

namespace D3D
{
CD3DFont font;

#define MAX_NUM_VERTICES 50*6
struct FONT2DVERTEX {
	float x,y,z;
	float rhw;
	u32 color;
	float tu, tv;
};

#define D3DFVF_FONT2DVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)
#define D3DFVF_FONT3DVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_NORMAL|D3DFVF_TEX1)

inline FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x=x; v.y=y; v.z=0; v.rhw=1.0f;  v.color = color;   v.tu = tu;   v.tv = tv;
	return v;
}

CD3DFont::CD3DFont()
{
	m_pTexture			   = NULL;
	m_pVB				   = NULL;
}

enum {m_dwTexWidth = 512, m_dwTexHeight = 512};

int CD3DFont::Init()
{
	// Create vertex buffer for the letters
	HRESULT hr;
	if (FAILED(hr = dev->CreateVertexBuffer(MAX_NUM_VERTICES*sizeof(FONT2DVERTEX),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &m_pVB, NULL)))
	{
		return hr;
	}
	m_fTextScale  = 1.0f; // Draw fonts into texture without scaling

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

	// Create a font.  By specifying ANTIALIASED_QUALITY, we might get an
	// antialiased font, but this is not guaranteed.
	// We definitely don't want to get it cleartype'd, anyway.
	int m_dwFontHeight = 24;
	int nHeight = -MulDiv(m_dwFontHeight, int(GetDeviceCaps(hDC, LOGPIXELSY) * m_fTextScale), 72);
	int dwBold = FW_NORMAL; ///FW_BOLD
	HFONT hFont = CreateFont(nHeight, 0, 0, 0, dwBold, 0,
		FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		VARIABLE_PITCH, _T("Tahoma"));
	if (NULL == hFont)
		return E_FAIL;

	HGDIOBJ hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor  (hDC, 0);
	SetTextAlign(hDC, TA_TOP);

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
			y += size.cy + 1;
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
	hr = dev->CreateTexture(m_dwTexWidth, m_dwTexHeight, 1, D3DUSAGE_DYNAMIC,
							D3DFMT_A4R4G4B4, D3DPOOL_DEFAULT, &m_pTexture, NULL);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create font texture");
		return hr;
	}

	// Lock the surface and write the alpha values for the set pixels
	D3DLOCKED_RECT d3dlr;
	m_pTexture->LockRect(0, &d3dlr, 0, D3DLOCK_DISCARD);
	int bAlpha; // 4-bit measure of pixel intensity

	for (y = 0; y < m_dwTexHeight; y++)
	{
		u16 *pDst16 = (u16*)((u8 *)d3dlr.pBits + y * d3dlr.Pitch);
		for (x = 0; x < m_dwTexWidth; x++)
		{
			bAlpha = ((pBitmapBits[m_dwTexWidth * y + x] & 0xff) >> 4);
			pDst16[x] = (bAlpha << 12) | 0x0fff;
		}
	}

	// Done updating texture, so clean up used objects
	m_pTexture->UnlockRect(0);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	return S_OK;
}

int CD3DFont::Shutdown()
{
	m_pVB->Release();
	m_pVB = NULL;
	m_pTexture->Release();
	m_pTexture = NULL;
	return S_OK;
}


const int RS[6][2] =
{
	{D3DRS_ALPHABLENDENABLE, TRUE},
	{D3DRS_SRCBLEND,		 D3DBLEND_SRCALPHA},
	{D3DRS_DESTBLEND,		 D3DBLEND_INVSRCALPHA},
	{D3DRS_CULLMODE,		 D3DCULL_NONE},
	{D3DRS_ZENABLE,			 FALSE},
	{D3DRS_FOGENABLE,		 FALSE},
};
const int TS[6][2] = 
{
	{D3DTSS_COLOROP,   D3DTOP_MODULATE},
	{D3DTSS_COLORARG1, D3DTA_TEXTURE},
	{D3DTSS_COLORARG2, D3DTA_DIFFUSE },
	{D3DTSS_ALPHAOP,   D3DTOP_MODULATE },
	{D3DTSS_ALPHAARG1, D3DTA_TEXTURE },
	{D3DTSS_ALPHAARG2, D3DTA_DIFFUSE },
};

void RestoreShaders()
{
	D3D::SetTexture(0, 0);
	D3D::RefreshStreamSource(0);
	D3D::RefreshIndices();
	D3D::RefreshVertexDeclaration();
	D3D::RefreshPixelShader();
	D3D::RefreshVertexShader();
}

void RestoreRenderStates()
{
	RestoreShaders();
	for (int i = 0; i < 6; i++)
	{
		D3D::RefreshRenderState((_D3DRENDERSTATETYPE)RS[i][0]);
		D3D::RefreshTextureStageState(0, (_D3DTEXTURESTAGESTATETYPE)int(TS[i][0]));
	}
}

void CD3DFont::SetRenderStates()
{
	D3D::SetTexture(0, m_pTexture);

	D3D::ChangePixelShader(0);
	D3D::ChangeVertexShader(0);
	D3D::ChangeVertexDeclaration(0);
	dev->SetFVF(D3DFVF_FONT2DVERTEX);

	for (int i = 0; i < 6; i++)
	{
		D3D::ChangeRenderState((_D3DRENDERSTATETYPE)RS[i][0], RS[i][1]);
		D3D::ChangeTextureStageState(0, (_D3DTEXTURESTAGESTATETYPE)int(TS[i][0]), TS[i][1]);
	}
}


int CD3DFont::DrawTextScaled(float x, float y, float fXScale, float fYScale, float spacing, u32 dwColor, const char* strText)
{
	if (!m_pVB)
		return 0;

	SetRenderStates();
	D3D::ChangeStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX));

	float vpWidth = 1;
	float vpHeight = 1;

	float sx = x*vpWidth-0.5f;
	float sy = y*vpHeight-0.5f;

	float fStartX = sx;

	float invLineHeight = 1.0f / ((m_fTexCoords[0][3] - m_fTexCoords[0][1]) * m_dwTexHeight);
	// Fill vertex buffer
	FONT2DVERTEX* pVertices;
	int dwNumTriangles = 0L;
	m_pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);

	const char *oldstrText=strText;
	//First, let's measure the text
	float tw=0;
	float mx=0;
	float maxx=0;

	while (*strText)
	{
		char c = *strText++;

		if (c == ('\n'))
			mx = 0;
		if (c < (' '))
			continue;

		float tx1 = m_fTexCoords[c-32][0];
		float tx2 = m_fTexCoords[c-32][2];

		float w = (tx2-tx1)*m_dwTexWidth;
		w *= (fXScale*vpHeight)*invLineHeight;
		mx += w + spacing*fXScale*vpWidth;
		if (mx > maxx) maxx = mx;
	}

	float offset = -maxx/2;
	strText = oldstrText;

	float wScale = (fXScale*vpHeight)*invLineHeight;
	float hScale = (fYScale*vpHeight)*invLineHeight;

	// Let's draw it
	while (*strText)
	{
		char c = *strText++;

		if (c == ('\n'))
		{
			sx  = fStartX;
			sy += fYScale*vpHeight;
		}
		if (c < (' '))
			continue;

		c -= 32;
		float tx1 = m_fTexCoords[c][0];
		float ty1 = m_fTexCoords[c][1];
		float tx2 = m_fTexCoords[c][2];
		float ty2 = m_fTexCoords[c][3];

		float w = (tx2-tx1)*m_dwTexWidth;
		float h = (ty2-ty1)*m_dwTexHeight;

		w *= wScale;
		h *= hScale;

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
			// Unlock, render, and relock the vertex buffer
			m_pVB->Unlock();
			dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);
			m_pVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);
			dwNumTriangles = 0;
		}
		sx += w + spacing*fXScale*vpWidth;
	}

	// Unlock and render the vertex buffer
	m_pVB->Unlock();
	if (dwNumTriangles > 0)
		dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, dwNumTriangles);
	RestoreRenderStates();
	return S_OK;
}

/* Explanation of texture copying via drawShadedTexQuad and drawShadedTexSubQuad:
	From MSDN: "When rendering 2D output using pre-transformed vertices,
				care must be taken to ensure that each texel area correctly corresponds
				to a single pixel area, otherwise texture distortion can occur."
	=> We need to subtract 0.5 from the vertex positions to properly map texels to pixels.
	HOWEVER, the MSDN article talks about D3DFVF_XYZRHW vertices, which bypass the programmable pipeline.
	Since we're using D3DFVF_XYZW and the programmable pipeline though, the vertex positions
	are normalized to [-1;+1], i.e. we need to scale the -0.5 offset by the texture dimensions.
	For example see a texture with a width of 640 pixels:
	"Expected" coordinate range when using D3DFVF_XYZRHW: [0;640]
	Normalizing this coordinate range for D3DFVF_XYZW: [0;640]->[-320;320]->[-1;1]
		i.e. we're subtracting width/2 and dividing by width/2
	BUT: The actual range when using D3DFVF_XYZRHW needs to be [-0.5;639.5] because of the need for exact texel->pixel mapping.
	We can still apply the same normalizing procedure though:
		[-0.5;639.5]->[-320-0.5;320-0.5]->[-1-0.5/320;1-0.5/320]

	So generally speaking the correct coordinate range is [-1-0.5/(w/2);1-0.5/(w/2)]
	which can be simplified to [-1-1/w;1-1/w].

	Note that while for D3DFVF_XYZRHW the y coordinate of the bottom of the screen is positive,
		it's negative for D3DFVF_XYZW. This is why we need to _add_ 1/h for the second position component instead of subtracting it.

	For a detailed explanation of this read the MSDN article "Directly Mapping Texels to Pixels (Direct3D 9)".
*/
void drawShadedTexQuad(IDirect3DTexture9 *texture,
					   const RECT *rSource,
					   int SourceWidth,
					   int SourceHeight,
					   int DestWidth,
					   int DestHeight,
					   IDirect3DPixelShader9 *PShader,
					   IDirect3DVertexShader9 *Vshader,
					   float Gamma)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float dw = 1.0f /(float) DestWidth;
	float dh = 1.0f /(float) DestHeight;
	float u1=((float)rSource->left) * sw;
	float u2=((float)rSource->right) * sw;
	float v1=((float)rSource->top) * sh;
	float v2=((float)rSource->bottom) * sh;
	float g = 1.0f/Gamma;

	const struct Q2DVertex { float x,y,z,rhw,u,v,w,h,G; } coords[4] = {
		{-1.0f - dw,-1.0f + dh, 0.0f,1.0f, u1, v2, sw, sh, g},
		{-1.0f - dw, 1.0f + dh, 0.0f,1.0f, u1, v1, sw, sh, g},
		{ 1.0f - dw,-1.0f + dh, 0.0f,1.0f, u2, v2, sw, sh, g},
		{ 1.0f - dw, 1.0f + dh, 0.0f,1.0f, u2, v1, sw, sh, g}
	};
	D3D::ChangeVertexShader(Vshader);
	D3D::ChangePixelShader(PShader);
	D3D::SetTexture(0, texture);
	D3D::ChangeVertexDeclaration(0);
	dev->SetFVF(D3DFVF_XYZW | D3DFVF_TEX3 |  D3DFVF_TEXCOORDSIZE1(2));
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, coords, sizeof(Q2DVertex));
	RestoreShaders();
}

void drawShadedTexSubQuad(IDirect3DTexture9 *texture,
							const MathUtil::Rectangle<float> *rSource,
							int SourceWidth,
							int SourceHeight,
							const MathUtil::Rectangle<float> *rDest,
							int DestWidth,
							int DestHeight,
							IDirect3DPixelShader9 *PShader,
							IDirect3DVertexShader9 *Vshader,
							float Gamma)
{
	float sw = 1.0f /(float) SourceWidth;
	float sh = 1.0f /(float) SourceHeight;
	float dw = 1.0f /(float) DestWidth;
	float dh = 1.0f /(float) DestHeight;
	float u1= rSource->left * sw;
	float u2= rSource->right * sw;
	float v1= rSource->top * sh;
	float v2= rSource->bottom * sh;
	float g = 1.0f/Gamma;

	struct Q2DVertex { float x,y,z,rhw,u,v,w,h,G; } coords[4] = {
		{ rDest->left  - dw , rDest->top    + dh, 1.0f,1.0f, u1, v2, sw, sh, g},
		{ rDest->left  - dw , rDest->bottom + dh, 1.0f,1.0f, u1, v1, sw, sh, g},
		{ rDest->right - dw , rDest->top    + dh, 1.0f,1.0f, u2, v2, sw, sh, g},
		{ rDest->right - dw , rDest->bottom + dh, 1.0f,1.0f, u2, v1, sw, sh, g}
	};
	D3D::ChangeVertexShader(Vshader);
	D3D::ChangePixelShader(PShader);
	D3D::SetTexture(0, texture);
	D3D::ChangeVertexDeclaration(0);
	dev->SetFVF(D3DFVF_XYZW | D3DFVF_TEX3 |  D3DFVF_TEXCOORDSIZE1(2));
	dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, coords, sizeof(Q2DVertex));
	RestoreShaders();
}

// Fills a certain area of the current render target with the specified color
// Z buffer disabled; destination coordinates normalized to (-1;1)
void drawColorQuad(u32 Color, float x1, float y1, float x2, float y2)
{
	struct CQVertex { float x, y, z, rhw; u32 col; } coords[4] = {
		{ x1, y2, 0.f, 1.f, Color },
		{ x2, y2, 0.f, 1.f, Color },
		{ x1, y1, 0.f, 1.f, Color },
		{ x2, y1, 0.f, 1.f, Color },
	};
	D3D::ChangeVertexShader(VertexShaderCache::GetClearVertexShader());
	D3D::ChangePixelShader(PixelShaderCache::GetClearProgram());
	D3D::ChangeVertexDeclaration(0);
	dev->SetFVF(D3DFVF_XYZW | D3DFVF_DIFFUSE);
	dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, coords, sizeof(CQVertex));
	RestoreShaders();
}

void drawClearQuad(u32 Color,float z,IDirect3DPixelShader9 *PShader,IDirect3DVertexShader9 *Vshader)
{
	struct Q2DVertex { float x,y,z,rhw;u32 color;} coords[4] = {
		{-1.0f,  1.0f, z, 1.0f, Color},
		{ 1.0f,  1.0f, z, 1.0f, Color},
		{ 1.0f, -1.0f, z, 1.0f, Color},
		{-1.0f, -1.0f, z, 1.0f, Color}
	};
	D3D::ChangeVertexShader(Vshader);
	D3D::ChangePixelShader(PShader);
	D3D::ChangeVertexDeclaration(0);
	dev->SetFVF(D3DFVF_XYZW | D3DFVF_DIFFUSE);
	dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, coords, sizeof(Q2DVertex));
	RestoreShaders();
}


}  // namespace

}  // namespace DX9
