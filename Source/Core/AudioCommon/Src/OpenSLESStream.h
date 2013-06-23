// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _OPENSLSTREAM_H_
#define _OPENSLSTREAM_H_

#include "Thread.h"
#include "SoundStream.h"

class OpenSLESStream : public SoundStream
{
#ifdef ANDROID
public:
	OpenSLESStream(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
	{};

	virtual ~OpenSLESStream() {};

	virtual bool Start();
	virtual void Stop();
	static bool isValid() { return true; }
	virtual bool usesMixer() const { return true; }

private:
	std::thread thread;
	Common::Event soundSyncEvent;
#else
public:
	OpenSLESStream(CMixer *mixer, void *hWnd = NULL): SoundStream(mixer) {}
#endif // HAVE_OPENSL
};

#endif
