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

#include "SoundStream.h"

class OpenSLESSoundStream: public CBaseSoundStream
{
#ifdef ANDROID
public:
	OpenSLESSoundStream(CMixer *mixer, void *hWnd = NULL);
	virtual ~OpenSLESSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual bool OnPreThreadStart() override;
	virtual void OnPreThreadJoin() override;

	static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
	
private:
	Common::Event m_soundSyncEvent;

	// engine interfaces
	SLObjectItf m_engineObject;
	SLEngineItf m_engineEngine;
	SLObjectItf m_outputMixObject;

	// buffer queue player interfaces
	SLObjectItf m_bqPlayerObject;
	SLPlayItf m_bqPlayerPlay;
	SLAndroidSimpleBufferQueueItf m_bqPlayerBufferQueue;
	SLMuteSoloItf m_bqPlayerMuteSolo;
	SLVolumeItf m_bqPlayerVolume;

	static const int BUFFER_SIZE = 512;
	static const int BUFFER_SIZE_IN_SAMPLES = (BUFFER_SIZE / 2);

	// Double buffering.
	short m_buffer[2][BUFFER_SIZE];
	int m_curBuffer;
#else
public:
	OpenSLESSoundStream(CMixer *mixer, void *hWnd = NULL):
		CBaseSoundStream(mixer)
	{
	}
#endif // ANDROID
};

#endif // _OPENSLSTREAM_H_
