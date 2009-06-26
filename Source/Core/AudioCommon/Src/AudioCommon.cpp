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

#include "AudioCommon.h"
#include "Mixer.h"
#include "DSoundStream.h"
#include "AOSoundStream.h"
#include "NullSoundStream.h"
#include "OpenALStream.h"

namespace AudioCommon 
{	
	SoundStream *InitSoundStream(CMixer *mixer) 
	{
		if (!mixer)
			mixer = new CMixer();

		std::string backend = ac_Config.sBackend;
		if (backend == BACKEND_DIRECTSOUND && DSound::isValid())       soundStream = new DSound(mixer, g_dspInitialize.hWnd);
		if (backend == BACKEND_AOSOUND     && AOSound::isValid())      soundStream = new AOSound(mixer);
		if (backend == BACKEND_OPENAL      && OpenALStream::isValid()) soundStream = new OpenALStream(mixer);
		if (backend == BACKEND_NULL        && NullSound::isValid())    soundStream = new NullSound(mixer);
		
		if (soundStream != NULL) {
			ac_Config.Update();
			if (soundStream->Start()) {
				// Start the sound recording
				/*
				  if (ac_Config.record) {
				  soundStream->StartLogAudio(FULL_DUMP_DIR g_Config.recordFile);
				  }
				*/

				return soundStream;
			}
			PanicAlert("Could not initialize backend %s, falling back to NULL", backend.c_str());
		}
			
		PanicAlert("Sound backend %s is not valid, falling back to NULL", backend.c_str());

		delete soundStream;
		soundStream = new NullSound(mixer);
		soundStream->Start();
		
		return NULL;
	}

	void ShutdownSoundStream() 
	{
		NOTICE_LOG(DSPHLE, "Shutting down sound stream");

		if (soundStream) 
		{
			soundStream->Stop();
			soundStream->StopLogAudio();
			delete soundStream;
			soundStream = NULL;
		}

		INFO_LOG(DSPHLE, "Done shutting down sound stream");	
	}

	std::vector<std::string> GetSoundBackends() 
	{
		std::vector<std::string> backends;

		if (DSound::isValid())       backends.push_back(BACKEND_DIRECTSOUND);
		if (AOSound::isValid())      backends.push_back(BACKEND_AOSOUND);
		if (OpenALStream::isValid()) backends.push_back(BACKEND_OPENAL);
		if (NullSound::isValid())    backends.push_back(BACKEND_NULL);
	   
		return backends;
	}
}
