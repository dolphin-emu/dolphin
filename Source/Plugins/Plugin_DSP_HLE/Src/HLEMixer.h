#ifndef HLEMIXER_H
#define HLEMIXER_H
#include "AudioCommon.h"
#include "Mixer.h"

class HLEMixer : public CMixer 
{
public:
	HLEMixer(unsigned int AISampleRate = 48000, unsigned int DSPSampleRate = 48000)
		: CMixer(AISampleRate, DSPSampleRate) {};
	
	virtual void Premix(short *samples, unsigned int numSamples, unsigned int sampleRate);	
};

#endif // HLEMIXER_H


