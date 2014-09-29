#pragma once

#include <string>
#include "AudioCommon/WaveFile.h"

struct AudioDumper
{
private:
	WaveFileWriter wfr;
	int currentrate;
	bool fileopen;
	int fileindex;
	std::string basename;
	bool checkem (int srate);
public:
	AudioDumper(const std::string& _basename);
	~AudioDumper ();

	void DumpSamples (const short *buff, int nsamp, int srate);
	void DumpSamplesBE (const short *buff, int nsamp, int srate);
};