// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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
