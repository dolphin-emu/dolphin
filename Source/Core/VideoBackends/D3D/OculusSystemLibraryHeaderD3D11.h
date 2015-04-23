// This is a header file for linking to the Oculus Rift system libraries to build for the Oculus Rift platform.
// Moral rights Carl Kenner
// Public Domain

#pragma once
#include "d3d11.h"
#include "VideoCommon/OculusSystemLibraryHeader.h"

typedef struct
{
	ovrTextureHeader Header;
	ID3D11Texture2D *pTexture;
	ID3D11ShaderResourceView *pSRView;
} ovrD3D11TextureData;

typedef union
{
	ovrD3D11TextureData D3D11;
	ovrTexture Texture;
} ovrD3D11Texture;

typedef struct
{
	ovrRenderAPIConfigHeader Header;
	ID3D11Device *pDevice;
	ID3D11DeviceContext *pDeviceContext;
	ID3D11RenderTargetView *pBackBufferRT;
	ID3D11UnorderedAccessView *pBackBufferUAV;
	IDXGISwapChain *pSwapChain;
} ovrD3D11ConfigData;

typedef union
{
	ovrD3D11ConfigData D3D11;
	ovrRenderAPIConfig Config;
} ovrD3D11Config;
