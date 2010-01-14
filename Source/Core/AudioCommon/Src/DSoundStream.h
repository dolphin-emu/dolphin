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

#ifndef _DSOUNDSTREAM_H_
#define _DSOUNDSTREAM_H_

#include "SoundStream.h"
#include "Thread.h"

#ifdef _WIN32
#include <mmsystem.h>
#include <dsound.h>

#define BUFSIZE (1024 * 8 * 4)
#endif

class DSound : public SoundStream
{
#ifdef _WIN32
    Common::Thread *thread;
    Common::EventEx soundSyncEvent;
    void  *hWnd;

    IDirectSound8* ds;
    IDirectSoundBuffer* dsBuffer;
    
    int bufferSize;     //i bytes
	int m_volume;
    
    // playback position
    int currentPos;
    int lastPos;
    short realtimeBuffer[BUFSIZE / sizeof(short)];
    
    inline int FIX128(int x)
	{
		return x & (~127);
    }

    inline int ModBufferSize(int x)
	{
		return (x + bufferSize) % bufferSize;
    }

    bool CreateBuffer();
    bool WriteDataToBuffer(DWORD dwOffset, char* soundData, DWORD dwSoundBytes);

public:
	DSound(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
		, bufferSize(0)
		, currentPos(0)
		, lastPos(0)
		, dsBuffer(0)
		, ds(0)
	{}

    virtual ~DSound() {}
 
	virtual bool Start();
    virtual void SoundLoop();
	virtual void SetVolume(int volume);
    virtual void Stop();
	virtual void Clear(bool mute);
    static bool isValid() { return true; }
    virtual bool usesMixer() const { return true; }
    virtual void Update();

#else
public:
	DSound(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
	{}
#endif
};

#endif //_DSOUNDSTREAM_H_
