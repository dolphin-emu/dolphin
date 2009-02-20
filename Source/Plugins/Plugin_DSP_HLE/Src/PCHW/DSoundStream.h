// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef __DSOUNDSTREAM_H__
#define __DSOUNDSTREAM_H__
#include "SoundStream.h"

#include "Thread.h"

#ifdef _WIN32
#include "stdafx.h"

#include <mmsystem.h>
#include <dsound.h>

#define BUFSIZE 32768
#define MAXWAIT 70   //ms

#endif

class DSound : public SoundStream
{
#ifdef _WIN32
    Common::Thread *thread;
    Common::CriticalSection *soundCriticalSection;
    Common::Event *soundSyncEvent;
    void  *hWnd;

    IDirectSound8* ds;
    IDirectSoundBuffer* dsBuffer;
    
    int bufferSize;     //i bytes
    int totalRenderedBytes;
    
    // playback position
    int currentPos;
    int lastPos;
    short realtimeBuffer[1024 * 1024];
    
    inline int FIX128(int x) {
		return x & (~127);
    }

    inline int ModBufferSize(int x) {
		return (x + bufferSize) % bufferSize;
    }

    bool CreateBuffer();
    bool WriteDataToBuffer(DWORD dwOffset, char* soundData,
			   DWORD dwSoundBytes);

public:
    DSound(int _sampleRate, StreamCallback _callback) :
        SoundStream(_sampleRate, _callback) {}
    
    DSound(int _sampleRate, StreamCallback _callback, void *_hWnd) :
	SoundStream(_sampleRate, _callback), hWnd(_hWnd) {}
    
    virtual ~DSound() {}
 
	virtual bool Start();
    virtual void SoundLoop();
    virtual void Stop();
    static bool isValid() { return true; }
    virtual bool usesMixer() const { return true; }
    virtual void Update();

#else
public:
    DSound(int _sampleRate, StreamCallback _callback, void *hWnd = NULL) :
        SoundStream(_sampleRate, _callback) {}
#endif
};

#endif //__DSOUNDSTREAM_H__
