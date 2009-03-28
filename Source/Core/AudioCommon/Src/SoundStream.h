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

#ifndef _SOUNDSTREAM_H_
#define _SOUNDSTREAM_H_

#include "Common.h"
#include "Mixer.h"
#include "WaveFile.h"

class SoundStream
{
protected:

	CMixer *m_mixer;
    // We set this to shut down the sound thread.
    // 0=keep playing, 1=stop playing NOW.
    volatile int threadData;
    bool m_logAudio;
	WaveFileWriter g_wave_writer;

public:   
	SoundStream(CMixer *mixer) : m_mixer(mixer), threadData(0) {}
	virtual ~SoundStream() { delete m_mixer;}
    
	static  bool isValid() { return false; }  
	virtual CMixer *GetMixer() const { return m_mixer; }
	virtual bool Start() { return false; }
	virtual void SoundLoop() {}
	virtual void Stop() {}
	virtual void Update() {}
	virtual void StartLogAudio(const char *filename) {
		if (! m_logAudio) {
			m_logAudio = true;
			g_wave_writer.Start(filename);
			g_wave_writer.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting Audio logging");
		} else {
			WARN_LOG(DSPHLE, "Audio logging already started");
		}
	}

	virtual void StopLogAudio() {
		if (m_logAudio) {
			m_logAudio = false;
			g_wave_writer.Stop();
			NOTICE_LOG(DSPHLE, "Starting Audio logging");
		} else {
			WARN_LOG(DSPHLE, "Audio logging already stopped");
		}
	}
};

#endif // _SOUNDSTREAM_H_
