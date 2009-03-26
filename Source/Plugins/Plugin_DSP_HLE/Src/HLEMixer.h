#ifndef HLEMIXER_H
#define HLEMIXER_H
#include "AudioCommon.h"
#include "Mixer.h"

class HLEMixer : public CMixer 
{
public:
	void MixUCode(short *samples, int numSamples);
	
	virtual void Premix(short *samples, int numSamples);	
};

#endif // HLEMIXER_H


