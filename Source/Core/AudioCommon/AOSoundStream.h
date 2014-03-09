// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
#include "Common/Thread.h"

#if defined(HAVE_AO) && HAVE_AO
#include <ao/ao.h>
#endif

class AOSound : public SoundStream
{
#if defined(HAVE_AO) && HAVE_AO
	std::thread thread;
	std::mutex soundCriticalSection;
	Common::Event soundSyncEvent;

	int buf_size;

	ao_device *device;
	ao_sample_format format;
	int default_driver;

	short realtimeBuffer[1024 * 1024];

public:
	AOSound(CMixer *mixer) : SoundStream(mixer) {}

	virtual ~AOSound();

	virtual bool Start() override;

	virtual void SoundLoop() override;

	virtual void Stop() override;

	static bool isValid() {
		return true;
	}

	virtual bool usesMixer() const {
		return true;
	}

	virtual void Update() override;

#else
public:
	AOSound(CMixer *mixer) : SoundStream(mixer) {}
#endif
};
