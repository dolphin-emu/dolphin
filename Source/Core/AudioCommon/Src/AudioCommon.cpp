#include "AudioCommon.h"
#include "Mixer.h"
#include "AOSoundStream.h"
#include "DSoundStream.h"
#include "NullSoundStream.h"
#include "OpenALStream.h"

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
	else if (backend == "OpenAL") {
		if (OpenALStream::isValid())
			soundStream = new OpenALStream(mixer);
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

void ShutdownSoundStream() {
	NOTICE_LOG(DSPHLE, "Shutting down sound stream");

	if (soundStream) {
		soundStream->Stop();
		soundStream->StopLogAudio();
		delete soundStream;
		soundStream = NULL;
	}
	
	// Check that soundstream already is stopped.
	while (soundStream) {
		ERROR_LOG(DSPHLE, "Waiting for sound stream");
		Common::SleepCurrentThread(2000);
	}
	INFO_LOG(DSPHLE, "Done shutting down sound stream");	
}

std::vector<std::string> GetSoundBackends() {
	std::vector<std::string> backends;
	// Add avaliable output options
	if (DSound::isValid())
		backends.push_back("DSound");
	if (AOSound::isValid())
		backends.push_back("AOSound");
	if (OpenALStream::isValid())
		backends.push_back("OpenAL");
	backends.push_back("NullSound");
   
	return backends;
}

} // Namespace
