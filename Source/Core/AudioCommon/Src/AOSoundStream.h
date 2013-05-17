// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _AOSOUNDSTREAM_H_
#define _AOSOUNDSTREAM_H_

#include "SoundStream.h"

#if defined(HAVE_AO) && HAVE_AO
#include <ao/ao.h>
#endif

#include "Thread.h"

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

	virtual bool Start();

	virtual void SoundLoop();

	virtual void Stop();

	static bool isValid() {
		return true;
	}

	virtual bool usesMixer() const { 
		return true; 
	}

	virtual void Update();

#else
public:
	AOSound(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif //_AOSOUNDSTREAM_H_
