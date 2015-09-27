// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "AudioCommon/Mixer.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

class SoundStream
{
protected:
	std::unique_ptr<CMixer> m_mixer;
	bool m_logAudio;
	WaveFileWriter g_wave_writer;
	bool m_muted;

public:
	SoundStream() : m_mixer(new CMixer(48000)), m_logAudio(false), m_muted(false) {}
	virtual ~SoundStream() { }

	static  bool isValid() { return false; }
	CMixer* GetMixer() const { return m_mixer.get(); }
	virtual bool Start() { return false; }
	virtual void SetVolume(int) {}
	virtual void SoundLoop() {}
	virtual void Stop() {}
	virtual void Update() {}
	virtual void Clear(bool mute) { m_muted = mute; }
	bool IsMuted() const { return m_muted; }

	void StartLogAudio(const std::string& filename)
	{
		if (!m_logAudio)
		{
			m_logAudio = true;
			g_wave_writer.Start(filename, m_mixer->GetSampleRate());
			g_wave_writer.SetSkipSilence(false);
			NOTICE_LOG(AUDIO, "Starting Audio logging");
		}
		else
		{
			WARN_LOG(AUDIO, "Audio logging already started");
		}
	}

	void StopLogAudio()
	{
		if (m_logAudio)
		{
			m_logAudio = false;
			g_wave_writer.Stop();
			NOTICE_LOG(AUDIO, "Stopping Audio logging");
		}
		else
		{
			WARN_LOG(AUDIO, "Audio logging already stopped");
		}
	}
};
