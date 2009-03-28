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

#ifndef _NULLSOUNDSTREAM_H_
#define _NULLSOUNDSTREAM_H_

#include "SoundStream.h"

class NullSound : public SoundStream
{   
public:
	NullSound(CMixer *mixer) : SoundStream(mixer) {}
    
    virtual ~NullSound() {}

    static bool isValid() {
        return true;
    }  

	virtual bool Start() { return true; }

	virtual void Update() { 
		m_mixer->Mix(NULL, 256 >> 2);
		//(*callback)(NULL, 256 >> 2, 16, sampleRate, 2); 
	}
};

#endif //_NULLSOUNDSTREAM_H_
