// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMutex>
#include <QObject>
#include <QSize>

// Singleton that talks to the Core via the interface defined in Core/Host.h.
// Because Host_* calls might come from different threads than the MainWindow,
// the Host class communicates with it via signals/slots only.

// Many of the Host_* functions are ignored, and some shouldn't exist.
class Host final : public QObject
{
	Q_OBJECT

public:
	static Host* GetInstance();

	void* GetRenderHandle();
	bool GetRenderFocus();
	bool GetRenderFullscreen();

public slots:
	void SetRenderHandle(void* handle);
	void SetRenderFocus(bool focus);
	void SetRenderFullscreen(bool fullscreen);

signals:
	void RequestTitle(const QString& title);
	void RequestStop();
	void RequestRenderSize(int w, int h);

private:
	Host() {}
	static Host* m_instance;
	QMutex m_lock;

	void* m_render_handle;
	bool m_render_focus;
	bool m_render_fullscreen;
};
