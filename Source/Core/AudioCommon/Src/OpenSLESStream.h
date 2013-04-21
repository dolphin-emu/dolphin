// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
