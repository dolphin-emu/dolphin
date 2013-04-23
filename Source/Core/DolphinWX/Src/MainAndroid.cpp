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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "Common.h"
#include "FileUtil.h"

#include "Core.h"
#include "Host.h"
#include "CPUDetect.h"
#include "Thread.h"

#include "PowerPC/PowerPC.h"
#include "HW/Wiimote.h"

#include "VideoBackendBase.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "BootManager.h"
#include "OnScreenDisplay.h"

#include "Android/ButtonManager.h"

#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
ANativeWindow* surf;
int g_width, g_height;
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Dolphinemu", __VA_ARGS__))

void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}

void Host_ShowJitResults(unsigned int address){}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
}

void* Host_GetRenderHandle()
{
	return surf;
}

void* Host_GetInstance() { return NULL; }

void Host_UpdateTitle(const char* title){};

void Host_UpdateLogDisplay(){}

void Host_UpdateDisasmDialog(){}

void Host_UpdateMainFrame()
{
}

void Host_UpdateBreakPointView(){}

bool Host_GetKeyState(int keycode)
{
	return false;
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	x = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos;
	y = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos;
	width = g_width; 	
	height = g_height; 
}

void Host_RequestRenderWindowSize(int width, int height) {}
void Host_SetStartupDebuggingParameters()
{
}

bool Host_RendererHasFocus()
{
	return true;
}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_SetWaitCursor(bool enable){}

void Host_UpdateStatusBar(const char* _pText, int Filed){}

void Host_SysMessage(const char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	size_t len = strlen(msg);
	if (msg[len - 1] != '\n') {
		msg[len - 1] = '\n';
		msg[len] = '\0';
	}
	LOGI(msg);
}

void Host_SetWiiMoteConnectionState(int _State) {}

void OSDCallbacks(u32 UserData)
{
	switch(UserData)
	{
		case 0: // Init
			ButtonManager::Init();
		break;
		case 1: // Draw
			ButtonManager::DrawButtons();
		break;
		case 2: // Shutdown
			ButtonManager::Shutdown();
		break;
		default:
			WARN_LOG(COMMON, "Error, wrong OSD type");
		break;
	}
}

#ifdef __cplusplus
extern "C"
{
#endif
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeGLSurfaceView_UnPauseEmulation(JNIEnv *env, jobject obj)
{
	PowerPC::Start();
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeGLSurfaceView_PauseEmulation(JNIEnv *env, jobject obj) 
{
	PowerPC::Pause();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeGLSurfaceView_StopEmulation(JNIEnv *env, jobject obj) 
{
	PowerPC::Stop();
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_DolphinEmulator_onTouchEvent(JNIEnv *env, jobject obj, jint Action, jfloat X, jfloat Y)
{
	ButtonManager::TouchEvent(Action, X, Y);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeGLSurfaceView_main(JNIEnv *env, jobject obj, jstring jFile, jobject _surf, jint _width, jint _height)
{
	surf = ANativeWindow_fromSurface(env, _surf);
	g_width = (int)_width;
	g_height = (int)_height;

	// Install our callbacks
	OSD::AddCallback(OSD::OSD_INIT, OSDCallbacks, 0);
	OSD::AddCallback(OSD::OSD_ONFRAME, OSDCallbacks, 1);
	OSD::AddCallback(OSD::OSD_SHUTDOWN, OSDCallbacks, 2);

	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);
	WiimoteReal::LoadSettings();
	
	const char *File = env->GetStringUTFChars(jFile, NULL);
	// No use running the loop when booting fails
	if ( BootManager::BootCore( File ) )
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
			updateMainFrameEvent.Wait();

	WiimoteReal::Shutdown();
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();
}
	  
#ifdef __cplusplus
}
#endif
