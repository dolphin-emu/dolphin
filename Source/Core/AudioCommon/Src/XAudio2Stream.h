// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _XAUDIO2STREAM_H_
#define _XAUDIO2STREAM_H_

#include "SoundStream.h"

#ifdef _WIN32

#include "Thread.h"
#include <xaudio2.h>
#include <memory>

struct StreamingVoiceContext: public IXAudio2VoiceCallback
{
public:
	StreamingVoiceContext(IXAudio2 *pXAudio2, CMixer *pMixer, Common::Event& pSyncEvent);
	~StreamingVoiceContext();
	
	void StreamingVoiceContext::Stop();
	void StreamingVoiceContext::Play();
	
	STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) {}
	STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
	STDMETHOD_(void, OnBufferStart) (void*) {}
	STDMETHOD_(void, OnLoopEnd) (void*) {}   
	STDMETHOD_(void, OnStreamEnd) () {}

	STDMETHOD_(void, OnBufferEnd) (void* context);

private:
	void SubmitBuffer(PBYTE buf_data);

private:
	CMixer *const m_mixer;
	Common::Event& m_sound_sync_event;
	IXAudio2SourceVoice* m_source_voice;
	std::unique_ptr<BYTE[]> xaudio_buffer;
};
#endif // _WIN32

class XAudio2SoundStream: public CBaseSoundStream
{
#ifdef _WIN32
public:
	XAudio2SoundStream(CMixer *mixer);
	virtual ~XAudio2SoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual void OnSetVolume(u32 volume) override;

	virtual void OnFlushBuffers(bool mute) override;
	
	virtual bool OnPreThreadStart() override;
	virtual void OnPreThreadJoin() override;

private:
	static float ConvertVolume(u32 volume)
	{
		//linear 1- .01
		return (float)volume / 100.0f;
	}

	void Stop();

private:
	class Releaser
	{
	public:
		template <typename R>
		void operator()(R* ptr)
		{
			ptr->Release();
		}
	};

private:
	std::unique_ptr<IXAudio2, Releaser> m_xaudio2;
	std::unique_ptr<StreamingVoiceContext> m_voice_context;
	IXAudio2MasteringVoice *m_mastering_voice;

	Common::Event m_sound_sync_event;

	const bool m_cleanup_com;

#else
public:
	XAudio2SoundStream(CMixer *mixer):
		CBaseSoundStream(mixer)
	{
	}
#endif // _WIN32
};

#endif //_XAUDIO2STREAM_H_
