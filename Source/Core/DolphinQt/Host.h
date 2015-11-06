// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMutex>

// Singleton that talks to the Core via the interface defined in Core/Host.h.
// Because Host_* calls might come from different threads than the MainWindow,
// the Host class communicates with it via signals/slots only.

// TODO only two Host_* calls are implemented here.
class Host final : public QObject
{
	Q_OBJECT

public:
	static Host* GetInstance();
	void DeleteHost();

	void* GetRenderHandle();

public slots:
	void SetRenderHandle(void* handle);

signals:
	void TitleUpdated(QString title);

private:
	Host() {}
	static Host* m_instance;
	void* m_render_handle;
	QMutex m_lock;
};
