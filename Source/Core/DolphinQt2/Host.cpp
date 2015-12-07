// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QMutexLocker>

#include "Common/Common.h"
#include "Core/Host.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/MainWindow.h"

Host* Host::m_instance = nullptr;

Host* Host::GetInstance()
{
	if (m_instance == nullptr)
		m_instance = new Host();
	return m_instance;
}

void* Host::GetRenderHandle()
{
	QMutexLocker locker(&m_lock);
	return m_render_handle;
}

void Host::SetRenderHandle(void* handle)
{
	QMutexLocker locker(&m_lock);
	m_render_handle = handle;
}

bool Host::GetRenderFocus()
{
	QMutexLocker locker(&m_lock);
	return m_render_focus;
}

void Host::SetRenderFocus(bool focus)
{
	QMutexLocker locker(&m_lock);
	m_render_focus = focus;
}

bool Host::GetRenderFullscreen()
{
	QMutexLocker locker(&m_lock);
	return m_render_fullscreen;
}

void Host::SetRenderFullscreen(bool fullscreen)
{
	QMutexLocker locker(&m_lock);
	m_render_fullscreen = fullscreen;
}

void Host_Message(int id)
{
	if (id == WM_USER_STOP)
		emit Host::GetInstance()->RequestStop();
}

void Host_UpdateTitle(const std::string& title)
{
	emit Host::GetInstance()->RequestTitle(QString::fromStdString(title));
}

void* Host_GetRenderHandle()
{
	return Host::GetInstance()->GetRenderHandle();
}

bool Host_RendererHasFocus()
{
	return Host::GetInstance()->GetRenderFocus();
}

bool Host_RendererIsFullscreen() {
	return Host::GetInstance()->GetRenderFullscreen();
}

// We ignore these, and their purpose should be questioned individually.
// In particular, RequestRenderWindowSize, RequestFullscreen, and
// UpdateMainFrame should almost certainly be removed.
void Host_UpdateMainFrame() {}
void Host_RequestFullscreen(bool enable) {}
void Host_RequestRenderWindowSize(int w, int h) {}
bool Host_UIHasFocus() { return false; }
void Host_NotifyMapLoaded() {}
void Host_UpdateDisasmDialog() {}
void Host_SetStartupDebuggingParameters() {}
void Host_SetWiiMoteConnectionState(int state) {}
void Host_ConnectWiimote(int wm_idx, bool connect) {}
void Host_ShowVideoConfig(void* parent, const std::string& backend_name,
                          const std::string& config_name) {}
void Host_RefreshDSPDebuggerWindow() {}
