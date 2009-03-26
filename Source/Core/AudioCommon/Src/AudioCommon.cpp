#include "AudioCommon.h"
#include "Mixer.h"
#include "AOSoundStream.h"
#include "DSoundStream.h"
#include "NullSoundStream.h"


namespace AudioCommon {
	
SoundStream *InitSoundStream(std::string backend, CMixer *mixer) {

	if (!mixer) {
		mixer = new CMixer();
	}

	if (backend == "DSound") {
		if (DSound::isValid())
			soundStream = new DSound(mixer, g_dspInitialize.hWnd);
	}
	else if (backend == "AOSound") {
		if (AOSound::isValid())
			soundStream = new AOSound(mixer);
	}
	else if (backend == "NullSound") {
		soundStream = new NullSound(mixer);
	} 
	else {
		PanicAlert("Cannot recognize backend %s", backend.c_str());
		return NULL;
	}
	
	if (soundStream) {
		if (!soundStream->Start()) {
			PanicAlert("Could not initialize backend %s, falling back to NULL", 
					   backend.c_str());
			delete soundStream;
			soundStream = new NullSound(mixer);
			soundStream->Start();
			}
	}
	else {
		PanicAlert("Sound backend %s is not valid, falling back to NULL", 
				   backend.c_str());
		delete soundStream;
		soundStream = new NullSound(mixer);
		soundStream->Start();
	}
	return soundStream;
}

} // Namespace
