// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _MIXER_H_
#define _MIXER_H_

#include "WaveFile.h"
#include "StdMutex.h"

// 16 bit Stereo
#define MAX_SAMPLES			(1024 * 8)
#define INDEX_MASK			(MAX_SAMPLES * 2 - 1)
#define RESERVED_SAMPLES	(256)

class CMixer {
	
public:
	CMixer(unsigned int AISampleRate = 48000, unsigned int DACSampleRate = 48000, unsigned int BackendSampleRate = 32000);
	virtual ~CMixer() {}

	// Called from audio threads
	virtual unsigned int Mix(short* samples, unsigned int numSamples);
	virtual void Premix(short * /*samples*/, unsigned int /*numSamples*/) {}

	// Called from main thread
	virtual void PushSamples(const short* samples, unsigned int num_samples);
	unsigned int GetSampleRate() {return m_sampleRate;}

	void SetThrottle(bool use) { m_throttle = use;}

	// TODO: do we need this
	bool IsHLEReady() { return m_HLEready;}
	void SetHLEReady(bool ready) { m_HLEready = ready;}
	// ---------------------


	virtual void StartLogAudio(const char *filename) {
		if (! m_logAudio) {
			m_logAudio = true;
			g_wave_writer.Start(filename, GetSampleRate());
			g_wave_writer.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting Audio logging");
		} else {
			WARN_LOG(DSPHLE, "Audio logging has already been started");
		}
	}

	virtual void StopLogAudio() {
		if (m_logAudio) {
			m_logAudio = false;
			g_wave_writer.Stop();
			NOTICE_LOG(DSPHLE, "Stopping Audio logging");
		} else {
			WARN_LOG(DSPHLE, "Audio logging has already been stopped");
		}
	}

	std::mutex& MixerCritical() { return m_csMixing; }

	float GetCurrentSpeed() const { return m_speed; }
	void UpdateSpeed(volatile float val) { m_speed = val; }

protected:
	unsigned int m_sampleRate;
	unsigned int m_aiSampleRate;
	unsigned int m_dacSampleRate;
	int m_bits;
	int m_channels;

	WaveFileWriter g_wave_writer;
	
	bool m_HLEready;
	bool m_logAudio;

	bool m_throttle;
	volatile u32 m_indexW;
	volatile u32 m_indexR;
	s16 m_last_sample[2];

	bool m_AIplaying;
	std::mutex m_csMixing;

	volatile float m_speed; // Current rate of the emulation (1.0 = 100% speed)
	float m_last_speed;
private:

};

#endif // _MIXER_H_
