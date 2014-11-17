// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstring>

#include <QApplication>
#include <QResizeEvent>

#include "Common/MsgHandler.h"
#include "Core/Host.h"

#include "DolphinQt/MainWindow.h"

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
