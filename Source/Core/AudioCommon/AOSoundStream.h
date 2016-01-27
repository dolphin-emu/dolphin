// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"
#include "Common/Thread.h"

#if defined(HAVE_AO) && HAVE_AO
#include <ao/ao.h>
#define MAX_AO_BUFFER 1024 * 512
#endif

class AOSound final : public SoundStream
{
#if defined(HAVE_AO) && HAVE_AO
	std::thread thread;
	std::atomic<bool> m_run_thread;
	std::mutex soundCriticalSection;
	Common::Event soundSyncEvent;

	int buf_size;

	ao_device *device;
	ao_sample_format format;
	int default_driver;

	float realtimeBuffer[MAX_AO_BUFFER];

public:
	bool Start() override;
	void SoundLoop() override;
	void Stop() override;
	void Update() override;

	static bool isValid()
	{
		return true;
	}

#endif
};
