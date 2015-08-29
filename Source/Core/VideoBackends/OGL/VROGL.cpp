// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include "VideoBackends/OGL/GLInterface/WGL.h"
#else
#include "VideoBackends/OGL/GLInterface/GLX.h"
#endif

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/TextureConverter.h"
#include "VideoBackends/OGL/VROGL.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

// Oculus Rift
#ifdef OVR_MAJOR_VERSION
ovrGLTexture g_eye_texture[2];
#endif

namespace OGL
{

#ifdef HAVE_OPENVR
GLuint m_left_texture = 0, m_right_texture = 0;
//-----------------------------------------------------------------------------
// Purpose: Creates all the shaders used by HelloVR SDL
//-----------------------------------------------------------------------------
bool CreateAllShaders()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BInitGL()
{
	if (!CreateAllShaders())
		return false;
	return true;
}
#endif


void VR_ConfigureHMD()
{
#ifdef HAVE_OPENVR
	if (m_pCompositor)
	{
		//m_pCompositor->SetGraphicsDevice(vr::Compositor_DeviceType_OpenGL, nullptr);
	}
#else
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
#if OVR_MAJOR_VERSION <= 5
		ovrGLConfig cfg;
		cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
#ifdef OCULUSSDK044ORABOVE
		cfg.OGL.Header.BackBufferSize.w = hmdDesc.Resolution.w;
		cfg.OGL.Header.BackBufferSize.h = hmdDesc.Resolution.h;
#else
		cfg.OGL.Header.RTSize.w = hmdDesc.Resolution.w;
		cfg.OGL.Header.RTSize.h = hmdDesc.Resolution.h;
#endif
		cfg.OGL.Header.Multisample = 0;
#ifdef _WIN32
		cfg.OGL.Window = (HWND)((cInterfaceWGL*)GLInterface)->m_window_handle;
		cfg.OGL.DC = GetDC(cfg.OGL.Window);
#ifndef OCULUSSDK042
#if OVR_MAJOR_VERSION <= 5
		if (g_is_direct_mode) //If in Direct Mode
		{
			ovrHmd_AttachToWindow(hmd, cfg.OGL.Window, nullptr, nullptr); //Attach to Direct Mode.
		}
#endif
#endif
#else
		cfg.OGL.Disp = (Display*)((cInterfaceGLX*)GLInterface)->getDisplay();
#ifdef OCULUSSDK043
		cfg.OGL.Win = glXGetCurrentDrawable();
#endif
#endif
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

#else
		for (int i = 0; i < ovrEye_Count; ++i)
			g_eye_render_desc[i] = ovrHmd_GetRenderDesc(hmd, (ovrEyeType)i, g_eye_fov[i]);
#endif
	}
#endif
#endif
}

void VR_StartFramebuffer(int target_width, int target_height, GLuint left_texture, GLuint right_texture)
{
#ifdef HAVE_OPENVR
	m_left_texture = left_texture;
	m_right_texture = right_texture;
#else
	if (g_has_vr920)
	{
#ifdef _WIN32
		VR920_StartStereo3D();
#endif
	}
#ifdef OVR_MAJOR_VERSION
	else if (g_has_rift)
	{
		g_eye_texture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
		g_eye_texture[0].OGL.Header.TextureSize.w = target_width;
		g_eye_texture[0].OGL.Header.TextureSize.h = target_height;
#if OVR_MAJOR_VERSION <= 5
		g_eye_texture[0].OGL.Header.RenderViewport.Pos.x = 0;
		g_eye_texture[0].OGL.Header.RenderViewport.Pos.y = 0;
		g_eye_texture[0].OGL.Header.RenderViewport.Size.w = target_width;
		g_eye_texture[0].OGL.Header.RenderViewport.Size.h = target_height;
#endif
		g_eye_texture[0].OGL.TexId = left_texture;
		g_eye_texture[1] = g_eye_texture[0];
		if (g_ActiveConfig.iStereoMode == STEREO_OCULUS)
			g_eye_texture[1].OGL.TexId = right_texture;
	}
#endif
#endif
}

void VR_StopFramebuffer()
{
#if defined(OVR_MAJOR_VERSION) && OVR_MAJOR_VERSION >= 6
	// On Oculus SDK 0.6.0 and above, we need to destroy the eye textures Oculus created for us.
#endif
}

void VR_BeginFrame()
{
	// At the start of a frame, we get the frame timing and begin the frame.
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
#if OVR_MAJOR_VERSION >= 6
		++g_ovr_frameindex;
		// On Oculus SDK 0.6.0 and above, we get the frame timing manually, then swap each eye texture 
		g_rift_frame_timing = ovrHmd_GetFrameTiming(hmd, 0);
		for (int eye = 0; eye < 2; eye++)
		{
			// Increment to use next texture, just before writing
			//pEyeRenderTexture[eye]->AdvanceToNextTexture();
		}
#else
		g_rift_frame_timing = ovrHmd_BeginFrame(hmd, ++g_ovr_frameindex);
#endif
}
#endif
}

void VR_PresentHMDFrame()
{
#ifdef HAVE_OPENVR
	if (m_pCompositor)
	{
		m_pCompositor->Submit(vr::Eye_Left, vr::API_OpenGL, (void*)m_left_texture, nullptr);
		m_pCompositor->Submit(vr::Eye_Right, vr::API_OpenGL, (void*)m_right_texture, nullptr);
		m_pCompositor->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
		uint32_t unSize = m_pCompositor->GetLastError(NULL, 0);
		if (unSize > 1)
		{
			char* buffer = new char[unSize];
			m_pCompositor->GetLastError(buffer, unSize);
			NOTICE_LOG(VR, "Compositor - %s\n", buffer);
			delete[] buffer;
		}
	}
#endif
#ifdef OVR_MAJOR_VERSION
	if (g_has_rift)
	{
		//ovrHmd_EndEyeRender(hmd, ovrEye_Left, g_left_eye_pose, &FramebufferManager::m_eye_texture[ovrEye_Left].Texture);
		//ovrHmd_EndEyeRender(hmd, ovrEye_Right, g_right_eye_pose, &FramebufferManager::m_eye_texture[ovrEye_Right].Texture);
#if OVR_MAJOR_VERSION <= 5
		// Let OVR do distortion rendering, Present and flush/sync.
		ovrHmd_EndFrame(hmd, g_eye_poses, &g_eye_texture[0].Texture);
#else
		ovrLayerEyeFov ld;
		ld.Header.Type = ovrLayerType_EyeFov;
		ld.Header.Flags = 0;
		for (int eye = 0; eye < 2; eye++)
		{
			//ld.ColorTexture[eye] = g_eye_texture[eye].Texture;
			//ld.Viewport[eye] = eyeRenderViewport[eye];
			ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = g_eye_poses[eye];
		}
		ovrLayerHeader* layers = &ld.Header;
		ovrResult result = ovrHmd_SubmitFrame(hmd, 0, nullptr, &layers, 1);
#endif
	}
#endif
}

void VR_DrawTimewarpFrame()
{
#ifdef OVR_MAJOR_VERSION
#if OVR_MAJOR_VERSION <= 5
	if (g_has_rift)
	{
		ovrFrameTiming frameTime = ovrHmd_BeginFrame(hmd, ++g_ovr_frameindex);

		ovr_WaitTillTime(frameTime.NextFrameSeconds - g_ActiveConfig.fTimeWarpTweak);

		ovrHmd_EndFrame(hmd, g_eye_poses, &g_eye_texture[0].Texture);
	}
#endif
#endif
}

void VR_DrawAsyncTimewarpFrame()
{
#ifdef OVR_MAJOR_VERSION
#if OVR_MAJOR_VERSION <= 5
	if (g_has_rift)
	{
		auto frameTime = ovrHmd_BeginFrame(hmd, ++g_ovr_frameindex);
		g_vr_lock.unlock();

		if (0 == frameTime.TimewarpPointSeconds) {
			ovr_WaitTillTime(frameTime.TimewarpPointSeconds - 0.002);
		}
		else {
			ovr_WaitTillTime(frameTime.NextFrameSeconds - 0.008);
		}

		g_vr_lock.lock();
		// Grab the most recent textures
		for (int eye = 0; eye < 2; eye++)
		{
			((ovrGLTexture&)(g_eye_texture[eye])).OGL.TexId = FramebufferManager::m_frontBuffer[eye];
		}
#ifdef _WIN32
		//HANDLE thread_handle = g_video_backend->m_video_thread->native_handle();
		//SuspendThread(thread_handle);
#endif
		ovrHmd_EndFrame(hmd, g_front_eye_poses, &g_eye_texture[0].Texture);
#ifdef _WIN32
		//ResumeThread(thread_handle);
#endif
	}
#endif
#endif
}

}