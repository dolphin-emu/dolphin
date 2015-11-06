// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QMutexLocker>

#include "Core/Host.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/MainWindow.h"

Host* Host::m_instance = nullptr;

Host* Host::GetInstance()
{
	if (m_instance == nullptr)
		m_instance = new Host();
	return m_instance;
}

void Host::DeleteHost()
{
	m_instance->deleteLater();
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

void Host_Message(int id) {}

void Host_UpdateMainFrame() {}

void Host_UpdateTitle(const std::string& title)
{
	emit Host::GetInstance()->TitleUpdated(QString::fromStdString(title));
}

void* Host_GetRenderHandle()
{
	return Host::GetInstance()->GetRenderHandle();
}

void Host_RequestRenderWindowSize(int w, int h) {}

bool Host_RendererHasFocus() { return false; }

bool Host_UIHasFocus() { return false; }

bool Host_RendererIsFullscreen() { return false; }

void Host_RequestFullscreen(bool enable) {}

void Host_NotifyMapLoaded() {}

void Host_UpdateDisasmDialog() {}

void Host_SetStartupDebuggingParameters() {}

void Host_SetWiiMoteConnectionState(int state) {}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_ShowVideoConfig(void* parent, const std::string& backend_name,
                          const std::string& config_name) {}

void Host_RefreshDSPDebuggerWindow() {}
