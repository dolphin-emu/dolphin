// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "OpenSLESStream.h"

#ifdef ANDROID

#include "Common.h"
#include <assert.h>

OpenSLESSoundStream::OpenSLESSoundStream(CMixer *mixer, void *hWnd /*= NULL*/):
	CBaseSoundStream(mixer),
	m_engineObject(NULL),
	m_engineEngine(NULL),
	m_outputMixObject(NULL),
	m_bqPlayerObject(NULL),
	m_bqPlayerPlay(NULL),
	m_bqPlayerBufferQueue(NULL),
	m_bqPlayerMuteSolo(NULL),
	m_bqPlayerVolume(NULL),
	m_buffer(),
	m_curBuffer(0)
{
}

OpenSLESSoundStream::~OpenSLESSoundStream()
{
}

bool OpenSLESSoundStream::OnPreThreadStart()
{
	SLresult result;
	// create engine
	result = slCreateEngine(&m_engineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engineEngine);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_engineEngine)->CreateOutputMix(m_engineEngine, &m_outputMixObject, 0, 0, 0);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
	SLDataFormat_PCM format_pcm =
	{
		SL_DATAFORMAT_PCM,
		2,
		SL_SAMPLINGRATE_44_1,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};

	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
	const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	result = (*m_engineEngine)->CreateAudioPlayer(m_engineEngine, &m_bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
	assert(SL_RESULT_SUCCESS == result);

	result = (*m_bqPlayerObject)->Realize(m_bqPlayerObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_bqPlayerObject)->GetInterface(m_bqPlayerObject, SL_IID_PLAY, &m_bqPlayerPlay);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_bqPlayerObject)->GetInterface(m_bqPlayerObject, SL_IID_BUFFERQUEUE, &m_bqPlayerBufferQueue);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_bqPlayerBufferQueue)->RegisterCallback(m_bqPlayerBufferQueue, &OpenSLESSoundStream::bqPlayerCallback, this);
	assert(SL_RESULT_SUCCESS == result);
	result = (*m_bqPlayerPlay)->SetPlayState(m_bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	assert(SL_RESULT_SUCCESS == result);

	// Render and enqueue a first buffer. (or should we just play the buffer empty?)
	m_curBuffer = 0;
	result = (*m_bqPlayerBufferQueue)->Enqueue(m_bqPlayerBufferQueue, m_buffer[m_curBuffer], sizeof(m_buffer[m_curBuffer]));
	if (SL_RESULT_SUCCESS != result)
	{
		return false;
	}
	m_curBuffer ^= 1;

	return true;
}

void OpenSLESSoundStream::OnPreThreadJoin()
{
	if (m_bqPlayerObject)
	{
		(*m_bqPlayerObject)->Destroy(m_bqPlayerObject);
		m_bqPlayerObject = NULL;
		m_bqPlayerPlay = NULL;
		m_bqPlayerBufferQueue = NULL;
		m_bqPlayerMuteSolo = NULL;
		m_bqPlayerVolume = NULL;
	}
	if (m_outputMixObject)
	{
		(*m_outputMixObject)->Destroy(m_outputMixObject);
		m_outputMixObject = NULL;
	}
	if (m_engineObject)
	{
		(*m_engineObject)->Destroy(m_engineObject);
		m_engineObject = NULL;
		m_engineEngine = NULL;
	}
}

void OpenSLESSoundStream::bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	assert(NULL != bq);
	assert(NULL != context);

	OpenSLESSoundStream *this_obj = static_cast<OpenSLESSoundStream*>(context);

	short *nextBuffer = this_obj->m_buffer[this_obj->m_curBuffer];
	int nextSize = sizeof(this_obj->m_buffer[0]);

	SLresult result = (*bq)->Enqueue(bq, nextBuffer, nextSize);

	// Comment from sample code:
	// the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
	// which for this code example would indicate a programming error
	assert(SL_RESULT_SUCCESS == result);

	this_obj->m_curBuffer ^= 1;	// Switch buffer
	// Render to the fresh buffer
	CMixer *mixer = this_obj->GetMixer();
	mixer->Mix(reinterpret_cast<short *>(this_obj->m_buffer[this_obj->m_curBuffer]), BUFFER_SIZE_IN_SAMPLES);
}

#endif
