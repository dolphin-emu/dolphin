#include <string>
#include <sstream>

#include "Common/Common.h"
#include "AudioDumper.h"
#include "Common/FileUtil.h"

/* automatically dumps to a series of files, splitting segments whenever sample rate (supplied per block) changes
or 2GB is reached
*/





/*
				if (ac_Config.m_DumpAudio)
				{
					std::string audio_file_name = File::GetUserPath(D_DUMPAUDIO_IDX) + "audiodump.wav";
					File::CreateFullPath(audio_file_name);
					mixer->StartLogAudio(audio_file_name.c_str());
				}
*/

AudioDumper::AudioDumper (std::string _basename)
{
	currentrate = 0;
	fileopen = false;
	fileindex = 0;
	basename = _basename;
}

AudioDumper::~AudioDumper ()
{
	if (fileopen)
	{
		wfr.Stop ();
		fileopen = false;
	}
}

bool AudioDumper::checkem (int srate)
{
	if (!fileopen || srate != currentrate || wfr.GetAudioSize () > 2 * 1000 * 1000 * 1000)
	{
		if (fileopen)
		{
			wfr.Stop ();
			fileopen = false;
		}
		std::stringstream fnames;
		fnames << File::GetUserPath(D_DUMPAUDIO_IDX) << basename << fileindex << ".wav";
		std::string fname = fnames.str ();
		if (!File::CreateFullPath (fname) || !wfr.Start (fname.c_str (), srate))
			// huh?
			return false;

		fileopen = true;
		currentrate = srate;
		fileindex++;
	}
	return true;

}

void AudioDumper::dumpsamplesBE (const short *buff, int nsamp, int srate)
{
	if (!checkem (srate))
		return;
	wfr.AddStereoSamplesBE (buff, nsamp);
}

void AudioDumper::dumpsamples (const short *buff, int nsamp, int srate)
{
	if (!checkem (srate))
		return;
	wfr.AddStereoSamples (buff, nsamp);
}







