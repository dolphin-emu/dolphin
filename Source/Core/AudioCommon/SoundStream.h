// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/Mixer.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"

class SoundStream
{
protected:

	CMixer* m_mixer;
	// We set this to shut down the sound thread.
	// 0=keep playing, 1=stop playing NOW.
	volatile int threadData;
	bool m_logAudio;
	WaveFileWriter g_wave_writer;
	bool m_muted;

public:
	SoundStream(CMixer* mixer) : m_mixer(mixer), threadData(0), m_logAudio(false), m_muted(false) {}
	virtual ~SoundStream() { delete m_mixer; }

	static  bool isValid() { return false; }
	virtual CMixer* GetMixer() const { return m_mixer; }
	virtual bool Start() { return false; }
	virtual void SetVolume(int) {}
	virtual void SoundLoop() {}
	virtual void Stop() {}
	virtual void Update() {}
	virtual void Clear(bool mute) { m_muted = mute; }
	bool IsMuted() const { return m_muted; }

	virtual void StartLogAudio(const std::string& filename)
	{
		if (!m_logAudio)
		{
			m_logAudio = true;
			g_wave_writer.Start(filename, m_mixer->GetSampleRate());
			g_wave_writer.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "Audio logging already started");
		}
	}

	virtual void StopLogAudio()
	{
		if (m_logAudio)
		{
			m_logAudio = false;
			g_wave_writer.Stop();
			NOTICE_LOG(DSPHLE, "Stopping Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "Audio logging already stopped");
		}
	}
};
