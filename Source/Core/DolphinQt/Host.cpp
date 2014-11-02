// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstring>

#include <QApplication>
#include <QResizeEvent>

#include "Common/MsgHandler.h"
#include "Core/Host.h"

#include "DolphinQt/MainWindow.h"

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

void Host_UpdateMainFrame()
{
	// TODO
}

void Host_UpdateTitle(const std::string& title)
{
	// TODO
}

void* Host_GetRenderHandle()
{
	return (void*)(g_main_window->GetRenderWidget()->winId());
}

void Host_GetRenderWindowSize(int& x, int& y, int& w, int& h)
{
	// TODO: Make it more clear what this is supposed to return.. i.e. WX always sets x=y=0
	g_main_window->RenderWidgetSize(x, y, w, h);
	x = 0;
	y = 0;
}

void Host_RequestRenderWindowSize(int w, int h)
{
	DRenderWidget* render_widget = g_main_window->GetRenderWidget();
	qApp->postEvent(render_widget, new QResizeEvent(QSize(w, h), render_widget->size()));
}

bool Host_RendererHasFocus()
{
	return g_main_window->RenderWidgetHasFocus();
}

bool Host_UIHasFocus()
{
	return g_main_window->isActiveWindow();
}

void Host_RequestFullscreen(bool enable)
{
	// TODO
}

void Host_NotifyMapLoaded()
{
	// TODO
}

void Host_UpdateDisasmDialog()
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
