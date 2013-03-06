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

#include <jni.h>
#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Dolphinemu", __VA_ARGS__))

bool rendererHasFocus = true;
bool running = true;

void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}

void Host_ShowJitResults(unsigned int address){}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
}

void* Host_GetRenderHandle()
{
	return NULL;
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
	width = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth;
	height = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight;
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

#ifdef __cplusplus
extern "C"
{
#endif
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_dolphinemuactivity_main(JNIEnv *env, jobject obj)
{
	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	VideoBackend::ActivateBackend(SConfig::GetInstance().
		m_LocalCoreStartupParameter.m_strVideoBackend);
	WiimoteReal::LoadSettings();

	// No use running the loop when booting fails
	if (BootManager::BootCore(""))
	{
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
			updateMainFrameEvent.Wait();
	}

	WiimoteReal::Shutdown();
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();
}
	  
#ifdef __cplusplus
}
#endif
