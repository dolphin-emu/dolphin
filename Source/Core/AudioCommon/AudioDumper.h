#ifndef _AUDIODUMPER_H
#define _AUDIODUMPER_H

#include "WaveFile.h"
#include <string>

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
	AudioDumper (std::string _basename);
	~AudioDumper ();

	void dumpsamples (const short *buff, int nsamp, int srate);
	void dumpsamplesBE (const short *buff, int nsamp, int srate);
};







#endif
