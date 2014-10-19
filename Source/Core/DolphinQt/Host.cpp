// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstring>

#include "Common/MsgHandler.h"
#include "Core/Host.h"

void Host_SysMessage(const char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n')
		msg[strlen(msg)-1] = '\0';
	PanicAlert("%s", msg);
}

void Host_Message(int id)
{
	// TODO
}

void* Host_GetRenderHandle()
{
	return nullptr;
}

void Host_NotifyMapLoaded()
{
	// TODO
}

void Host_UpdateDisasmDialog()
{
	// TODO
}

void Host_UpdateMainFrame()
{
	// TODO
}

void Host_UpdateTitle(const std::string& title)
{
	// TODO
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	// TODO
}

void Host_RequestRenderWindowSize(int width, int height)
{
	// TODO
}

void Host_RequestFullscreen(bool enable_fullscreen)
{
	// TODO
}

void Host_SetStartupDebuggingParameters()
{
	// TODO
}

void Host_SetWiiMoteConnectionState(int state)
{
	// TODO
}

bool Host_UIHasFocus()
{
	// TODO
	return false;
}

bool Host_RendererHasFocus()
{
	// TODO
	return false;
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	// TODO
}

void Host_ShowVideoConfig(void* parent, const std::string& backend_name,
                          const std::string& config_name)
{
	// TODO
}

void Host_RefreshDSPDebuggerWindow()
{
	// TODO
}
