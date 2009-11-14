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

#include "Common.h"

#include "D3DBase.h"
#include "D3DUtil.h"
#include "Render.h"

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
	m_pTexture             = NULL;
	m_pVB                  = NULL;
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
	int *pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader,  sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       =  (int)m_dwTexWidth;
	bmi.bmiHeader.biHeight      = -(int)m_dwTexHeight;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount    = 32;

	// Create a DC and a bitmap for the font
	HDC     hDC       = CreateCompatibleDC(NULL);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (VOID**)&pBitmapBits, NULL, 0);
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

	// Loop through all printable character and output them to the bitmap..
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

		x += size.cx + 3;  //3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
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
	{D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA},
	{D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA},
	{D3DRS_CULLMODE,         D3DCULL_NONE},
	{D3DRS_ZENABLE,          FALSE},
	{D3DRS_FOGENABLE,        FALSE},
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

static DWORD RS_old[6];
static DWORD TS_old[6];
static LPDIRECT3DBASETEXTURE9 texture_old;
static LPDIRECT3DPIXELSHADER9 ps_old;
static LPDIRECT3DVERTEXSHADER9 vs_old;

void SaveRenderStates()
{
	// TODO: Get rid of these Gets so we can potentially switch to Pure Device
	for (int i = 0; i < 6; i++)
	{
		dev->GetRenderState((_D3DRENDERSTATETYPE)RS[i][0], &(RS_old[i]));
		dev->GetTextureStageState(0, (_D3DTEXTURESTAGESTATETYPE)int(TS[i][0]), &(TS_old[i]));
	}
	dev->GetTexture(0, &texture_old);
	dev->GetPixelShader(&ps_old);
	dev->GetVertexShader(&vs_old);
}

void RestoreRenderStates()
{
	if(texture_old)
	{
		D3D::SetTexture(0, texture_old);
		texture_old->Release();
		texture_old = NULL;
	}
	if(ps_old)
	{
		dev->SetPixelShader(ps_old);
		ps_old->Release();
		ps_old = NULL;
	}
	if(vs_old)
	{
		dev->SetVertexShader(vs_old);
		vs_old->Release();
		vs_old = NULL;
	}
	
	for (int i = 0; i < 6; i++)
	{
		D3D::SetRenderState((_D3DRENDERSTATETYPE)RS[i][0], RS_old[i]);
		D3D::SetTextureStageState(0, (_D3DTEXTURESTAGESTATETYPE)int(TS[i][0]), TS_old[i]);
	}
}

void CD3DFont::SetRenderStates()
{
	D3D::SetTexture(0, m_pTexture);

	dev->SetPixelShader(0);
	dev->SetVertexShader(0);
	
	D3D::SetVertexDeclaration(NULL);  // throw away cached vtx decl
	dev->SetFVF(D3DFVF_FONT2DVERTEX);

	for (int i = 0; i < 6; i++)
	{
		D3D::SetRenderState((_D3DRENDERSTATETYPE)RS[i][0], RS[i][1]);
		D3D::SetTextureStageState(0, (_D3DTEXTURESTAGESTATETYPE)int(TS[i][0]), TS[i][1]);
	}
}


int CD3DFont::DrawTextScaled(float x, float y, float fXScale, float fYScale, float spacing, u32 dwColor, const char* strText, bool center)
{
	if (!m_pVB)
		return 0;

	SaveRenderStates();
	SetRenderStates();
	dev->SetStreamSource(0, m_pVB, 0, sizeof(FONT2DVERTEX));

	float vpWidth = 1;
	float vpHeight = 1;

	float sx = x*vpWidth-0.5f; 
	float sy = y*vpHeight-0.5f;

	float fStartX = sx;

	float invLineHeight = 1.0f / ((m_fTexCoords[0][3] - m_fTexCoords[0][1]) * m_dwTexHeight);
	// Fill vertex buffer
	FONT2DVERTEX* pVertices;
	int         dwNumTriangles = 0L;
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
	//Then let's draw it
	if (center)
	{
		sx+=offset;
		fStartX+=offset;
	}

	float wScale = (fXScale*vpHeight)*invLineHeight;
	float hScale = (fYScale*vpHeight)*invLineHeight;

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

		c-=32;
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

void quad2d(float x1, float y1, float x2, float y2, u32 color, float u1, float v1, float u2, float v2)
{
	SaveRenderStates();	
	struct Q2DVertex { float x,y,z,rhw; u32 color; float u, v; } coords[4] = {
		{x1-0.5f, y1-0.5f, 0, 1, color, u1, v1},
		{x2-0.5f, y1-0.5f, 0, 1, color, u2, v1},
		{x2-0.5f, y2-0.5f, 0, 1, color, u2, v2},
		{x1-0.5f, y2-0.5f, 0, 1, color, u1, v2},
	};
	dev->SetPixelShader(0);
	dev->SetVertexShader(0);
	dev->SetVertexDeclaration(0);
	
	D3D::SetVertexDeclaration(NULL);  // throw away cached vtx decl
	dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, coords, sizeof(Q2DVertex));
	
	RestoreRenderStates();
}

void drawShadedTexQuad(IDirect3DTexture9 *texture,
					   const RECT *rSource,
					   int SourceWidth,
					   int SourceHeight,
					   const RECT *rDest,
					   IDirect3DPixelShader9 *PShader,
					   IDirect3DVertexShader9 *Vshader)
{
	SaveRenderStates();
	
	//float span = ((rSource->right-rSource->left - 1.0f) * (rDest->right - rDest->left))/(SourceWidth*((rDest->right - rDest->left)-1.0f));
	float u1=((float)rSource->left+1.0f)/(float) SourceWidth;//*/((0.5f+rSource->left)/(float) SourceWidth)-(span*0.5f/(float)(rDest->right - rDest->left));
	float u2=((float)rSource->right-1.0f)/(float) SourceWidth;;//*/u1+span;  
	//span = ((rSource->bottom-rSource->top - 1.0f) * (rDest->bottom - rDest->top))/(SourceHeight*((rDest->bottom - rDest->top)-1.0f));
	float v1=((float)rSource->top+1.0f)/(float) SourceHeight;//*/((0.5f+rSource->top)/(float) SourceHeight)-(span*0.5f/(float)(rDest->bottom - rDest->top));
	float v2=((float)rSource->bottom-1.0f)/(float) SourceHeight;//*/v1+span; 

	struct Q2DVertex { float x,y,z,rhw,u,v; } coords[4] = {
		{(float)rDest->left-0.5f, (float)rDest->top-0.5f, 0.0f, 1.0f, u1, v1},
		{(float)rDest->right-0.5f, (float)rDest->top-0.5f, 0.0f,1.0f, u2, v1},
		{(float)rDest->right-0.5f, (float)rDest->bottom-0.5f, 0.0f,1.0f, u2, v2},
		{(float)rDest->left-0.5f, (float)rDest->bottom-0.5f, 0.0f,1.0f, u1, v2}
	};
	dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	dev->SetVertexShader(Vshader);
	dev->SetPixelShader(PShader);	
	dev->SetTexture(0, texture);
	dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, coords, sizeof(Q2DVertex));	
	D3D::RefreshVertexDeclaration();
	RestoreRenderStates();

}	

}  // namespace
