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

#ifndef __SOUNDSTREAM_H__
#define __SOUNDSTREAM_H__

#include "Common.h"
#include "Mixer.h"

class SoundStream
{
protected:

	CMixer *m_mixer;
    // We set this to shut down the sound thread.
    // 0=keep playing, 1=stop playing NOW.
    volatile int threadData;
    
public:   
	SoundStream(CMixer *mixer) : m_mixer(mixer), threadData(0) {}
	virtual ~SoundStream() { delete m_mixer;}
    
	static  bool isValid() { return false; }  
	virtual CMixer *GetMixer() const { return m_mixer; }
	virtual bool Start() { return false; }
	virtual void SoundLoop() {}
	virtual void Stop() {}
	virtual void Update() {}
};

#endif
