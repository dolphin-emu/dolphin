// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VRD3D.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

// Oculus Rift
#ifdef OVR_MAJOR_VERSION
ovrD3D11Texture g_eye_texture[2];
#endif

namespace DX11
{

void VR_ConfigureHMD()
{
#ifdef OVR_MAJOR_VERSION_
	ovrD3D11Config cfg;
	cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
#ifdef OCULUSSDK044ORABOVE
	cfg.D3D11.Header.BackBufferSize.w = hmdDesc.Resolution.w;
	cfg.D3D11.Header.BackBufferSize.h = hmdDesc.Resolution.h;
#else
	cfg.D3D11.Header.RTSize.w = hmdDesc.Resolution.w;
	cfg.D3D11.Header.RTSize.h = hmdDesc.Resolution.h;
#endif
	cfg.D3D11.Header.Multisample = 0;
	cfg.D3D11.pDevice = D3D::device;
	cfg.D3D11.pDeviceContext = D3D::context;
	cfg.D3D11.pSwapChain = D3D::swapchain;
	cfg.D3D11.pBackBufferRT = D3D::GetBackBuffer()->GetRTV();
	if (g_is_direct_mode) //If Rift is in Direct Mode
	{
		//To do: This is a bit of a hack, but I haven't found any problems with this.  
		//If we don't want to do this, large changes will be needed to init sequence.
		D3D::UnloadDXGI();  //Unload CreateDXGIFactory() before ovrHmd_AttachToWindow, or else direct mode won't work.
		ovrHmd_AttachToWindow(hmd, D3D::hWnd, nullptr, nullptr); //Attach to Direct Mode.
		D3D::LoadDXGI();
	}
	int caps = 0;
#if OVR_MAJOR_VERSION <= 4
	if (g_Config.bChromatic)
		caps |= ovrDistortionCap_Chromatic;
#endif
	if (g_Config.bTimewarp)
		caps |= ovrDistortionCap_TimeWarp;
	if (g_Config.bVignette)
		caps |= ovrDistortionCap_Vignette;
	if (g_Config.bNoRestore)
		caps |= ovrDistortionCap_NoRestore;
	if (g_Config.bFlipVertical)
		caps |= ovrDistortionCap_FlipInput;
	if (g_Config.bSRGB)
		caps |= ovrDistortionCap_SRGB;
	if (g_Config.bOverdrive)
		caps |= ovrDistortionCap_Overdrive;
	if (g_Config.bHqDistortion)
		caps |= ovrDistortionCap_HqDistortion;
	ovrHmd_ConfigureRendering(hmd, &cfg.Config, caps,
		g_eye_fov, g_eye_render_desc);
#if OVR_MAJOR_VERSION <= 4
	ovrhmd_EnableHSWDisplaySDKRender(hmd, false); //Disable Health and Safety Warning.
#endif
#endif
}

void VR_StartFramebuffer()
{
	if (g_has_vr920)
	{
#ifdef _WIN32
		VR920_StartStereo3D();
#endif
	}
#ifdef OVR_MAJOR_VERSION
	else if (g_has_rift)
	{
		g_eye_texture[0].D3D11.Header.API = ovrRenderAPI_D3D11;
		g_eye_texture[0].D3D11.Header.TextureSize.w = Renderer::GetTargetWidth();
		g_eye_texture[0].D3D11.Header.TextureSize.h = Renderer::GetTargetHeight();
		g_eye_texture[0].D3D11.Header.RenderViewport.Pos.x = 0;
		g_eye_texture[0].D3D11.Header.RenderViewport.Pos.y = 0;
		g_eye_texture[0].D3D11.Header.RenderViewport.Size.w = Renderer::GetTargetWidth();
		g_eye_texture[0].D3D11.Header.RenderViewport.Size.h = Renderer::GetTargetHeight();
		g_eye_texture[0].D3D11.pTexture = FramebufferManager::m_efb.m_frontBuffer[0]->GetTex();
		g_eye_texture[0].D3D11.pSRView = FramebufferManager::m_efb.m_frontBuffer[0]->GetSRV();
		g_eye_texture[1] = g_eye_texture[0];
		if (g_ActiveConfig.iStereoMode == STEREO_OCULUS)
		{
			g_eye_texture[1].D3D11.pTexture = FramebufferManager::m_efb.m_frontBuffer[1]->GetTex();
			g_eye_texture[1].D3D11.pSRView = FramebufferManager::m_efb.m_frontBuffer[1]->GetSRV();
		}
	}
#endif
}

void VR_PresentHMDFrame()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		//ovrHmd_EndEyeRender(hmd, ovrEye_Left, g_left_eye_pose, &FramebufferManager::m_eye_texture[ovrEye_Left].Texture);
		//ovrHmd_EndEyeRender(hmd, ovrEye_Right, g_right_eye_pose, &FramebufferManager::m_eye_texture[ovrEye_Right].Texture);

		//Change to compatible D3D Blend State:
		//Some games (e.g. Paper Mario) do not use a Blend State that is compatible
		//with the Oculus Rift's SDK.  They set RenderTargetWriteMask to 0,
		//which masks out the call's Pixel Shader stage.  This also seems inefficient
		// from a rendering point of view.  Could this be an area Dolphin could be optimized?
		//To Do: Only use this when needed?  Is this slow?
		ID3D11BlendState* g_pOculusRiftBlendState = NULL;

		D3D11_BLEND_DESC oculusBlendDesc;
		ZeroMemory(&oculusBlendDesc, sizeof(D3D11_BLEND_DESC));
		oculusBlendDesc.AlphaToCoverageEnable = FALSE;
		oculusBlendDesc.IndependentBlendEnable = FALSE;
		oculusBlendDesc.RenderTarget[0].BlendEnable = FALSE;
		oculusBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		HRESULT hr = D3D::device->CreateBlendState(&oculusBlendDesc, &g_pOculusRiftBlendState);
		if (FAILED(hr)) PanicAlert("Failed to create blend state at %s %d\n", __FILE__, __LINE__);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)g_pOculusRiftBlendState, "blend state used to make sure rift draw call works");

		D3D::context->OMSetBlendState(g_pOculusRiftBlendState, NULL, 0xFFFFFFFF);

		// Let OVR do distortion rendering, Present and flush/sync.
		ovrHmd_EndFrame(hmd, g_eye_poses, &g_eye_texture[0].Texture);
	}
#endif
}

void VR_DrawTimewarpFrame()
{
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		ovrFrameTiming frameTime = ovrHmd_BeginFrame(hmd, ++g_ovr_frameindex);
		//const ovrTexture* new_eye_texture = new ovrTexture(FramebufferManager::m_eye_texture[0].Texture);
		//ovrD3D11Texture new_eye_texture;
		//memcpy((void*)&new_eye_texture, &FramebufferManager::m_eye_texture[0], sizeof(ovrD3D11Texture));

		//ovrPosef new_eye_poses[2];
		//memcpy((void*)&new_eye_poses, g_eye_poses, sizeof(ovrPosef)*2);

		ovr_WaitTillTime(frameTime.NextFrameSeconds - g_ActiveConfig.fTimeWarpTweak);

		ovrHmd_EndFrame(hmd, g_eye_poses, &g_eye_texture[0].Texture);
	}
#endif
}

}