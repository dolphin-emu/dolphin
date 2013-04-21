// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _SOUNDSTREAM_H_
#define _SOUNDSTREAM_H_

#include "Common.h"
#include "Mixer.h"
#include "WaveFile.h"

#include "Thread.h"
#include <assert.h>

class CBaseSoundStream: public NonCopyable
{
public:
	CBaseSoundStream(CMixer *mixer);
	virtual ~CBaseSoundStream();
	
	static inline bool IsValid() { return false; }

	bool	StartThread();
	void	StopThread();
	bool	IsRunning();

	void	SetPaused(bool paused);
	void	SetThrottle(bool throttle);

	void	SetMuted(bool muted);
	bool	IsMuted() const;

	void	SetVolume(u32 volume);
	u32		GetVolume() const;

	void	StartRecordAudio(const char *filename);
	void	StopRecordAudio();

	void	Update();
	void	FlushBuffers(bool mute);

	void	PushSamples(const short *samples, unsigned int num_samples);
	void	SetMixSpeed(float speed);

	void	SetHLEReady(bool ready);
	bool	IsHLEReady() const;

	u32		GetSampleRate() const;

protected:
	CMixer	*GetMixer() const;

// override these if you want
private:
	virtual void OnSetMuted(bool muted) {}
	virtual void OnSetVolume(u32 volume) {}

	virtual void OnUpdate() {}
	virtual void OnFlushBuffers(bool mute) {}

	virtual bool OnPreThreadStart() { return true; }
	virtual void OnPostThreadStart() {}
	virtual void SoundLoop() {}
	virtual void OnPreThreadJoin() {}
	virtual void OnPostThreadJoin() {}

private:
	inline void ThreadFuncInternal() { SoundLoop(); }

private:
	std::thread m_thread;
	std::unique_ptr<CMixer> m_mixer;
	u32 m_volume;
	bool m_muted;
};

inline CBaseSoundStream::CBaseSoundStream(CMixer *mixer):
	m_mixer(mixer),
	m_volume(0),
	m_muted(false)
{
	if (!mixer)
	{
		throw;
	}
}

inline CBaseSoundStream::~CBaseSoundStream()
{
}

inline bool CBaseSoundStream::StartThread()
{
	bool ret = OnPreThreadStart();
	if (ret)
	{
		m_thread = std::thread(std::mem_fun(&CBaseSoundStream::ThreadFuncInternal), this);
		OnPostThreadStart();
	}
	return ret;
}

inline void CBaseSoundStream::StopThread()
{
	// might need to change the sync location in the sound loops
	// to prevent a deadlock
	OnPreThreadJoin();
	m_thread.join();
	OnPostThreadJoin();
}

inline bool CBaseSoundStream::IsRunning()
{
	return m_thread.joinable();
}

inline void CBaseSoundStream::SetPaused(bool pause)
{
	if (pause)
	{
		m_mixer->MixerCritical().lock();
	}
	else
	{
		m_mixer->MixerCritical().unlock();
	}
}

inline void CBaseSoundStream::SetThrottle(bool throttle)
{
	m_mixer->SetThrottle(throttle);
}

inline void CBaseSoundStream::SetMuted(bool muted)
{
	m_muted = muted;
	OnSetMuted(m_muted);
}

inline bool CBaseSoundStream::IsMuted() const
{
	return m_muted;
}

inline CMixer *CBaseSoundStream::GetMixer() const
{
	return m_mixer.get();
}

inline void CBaseSoundStream::SetVolume(u32 volume)
{
	m_volume = volume % 101;
	OnSetVolume(m_volume);
}

inline u32 CBaseSoundStream::GetVolume() const
{
	return m_volume;
}

inline void CBaseSoundStream::StartRecordAudio(const char *filename)
{
	m_mixer->StartLogAudio(filename);
}

inline void CBaseSoundStream::StopRecordAudio()
{
	m_mixer->StopLogAudio();
}

inline void CBaseSoundStream::Update()
{
	OnUpdate();
}

inline void CBaseSoundStream::FlushBuffers(bool mute)
{
	SetMuted(mute);
	OnFlushBuffers(mute);
}

inline void CBaseSoundStream::PushSamples(const short *samples, unsigned int num_samples)
{
	m_mixer->PushSamples(samples, num_samples);
}

inline void CBaseSoundStream::SetMixSpeed(float speed)
{
	m_mixer->UpdateSpeed(speed);
}

inline void CBaseSoundStream::SetHLEReady(bool ready)
{
	m_mixer->SetHLEReady(ready);
}

inline bool CBaseSoundStream::IsHLEReady() const
{
	return m_mixer->IsHLEReady();
}

inline u32 CBaseSoundStream::GetSampleRate() const
{
	return m_mixer->GetSampleRate();
}

#endif // _SOUNDSTREAM_H_
