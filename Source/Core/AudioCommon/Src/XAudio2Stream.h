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

#ifndef _XAUDIO2STREAM_H_
#define _XAUDIO2STREAM_H_

#include "SoundStream.h"

#ifdef _WIN32
#include "Thread.h"
#include <xaudio2.h>

const int NUM_BUFFERS = 3;
const int SAMPLES_PER_BUFFER = 96;

const int NUM_CHANNELS = 2;
const int BUFFER_SIZE = SAMPLES_PER_BUFFER * NUM_CHANNELS;
const int BUFFER_SIZE_BYTES = BUFFER_SIZE * sizeof(s16);


#ifndef safe_delete_array
#define safe_delete_array(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef safe_delete
#define safe_delete(a) if( (a) != NULL ) delete (a); (a) = NULL;
#endif
#ifndef safe_release
#define safe_release(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif


#endif

class XAudio2 : public SoundStream
{
#ifdef _WIN32
	IXAudio2 *pXAudio2;
	IXAudio2MasteringVoice *pMasteringVoice;
	IXAudio2SourceVoice *pSourceVoice;

	Common::EventEx soundSyncEvent;
	float m_volume;


	bool Init();
public:
	XAudio2(CMixer *mixer) 
		: SoundStream(mixer),
		pXAudio2(0),
		pMasteringVoice(0),
		pSourceVoice(0),
		m_volume(1.0f) {}

    virtual ~XAudio2() {}
 
	virtual bool Start();
	virtual void SetVolume(int volume);
    virtual void Stop();
	virtual void Clear(bool mute);
    static bool isValid() { return true; }
    virtual bool usesMixer() const { return true; }
    virtual void Update();

#else
public:
	XAudio2(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
	{}
#endif
};

#endif //_XAUDIO2STREAM_H_
