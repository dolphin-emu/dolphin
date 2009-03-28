#include "Config.h" // Local
#include "Globals.h"
#include "DSPHandler.h"
#include "HLEMixer.h"

void HLEMixer::MixUCode(short *samples, int numSamples) {
	// if this was called directly from the HLE, and not by timeout
	if (g_Config.m_EnableHLEAudio && IsHLEReady()) {
		IUCode* pUCode = CDSPHandler::GetInstance().GetUCode();
		if (pUCode != NULL)
			pUCode->MixAdd(samples, numSamples);
	}
}

void HLEMixer::Premix(short *samples, int numSamples) {
	
	// first get the DTK Music
	if (g_Config.m_EnableDTKMusic) {
		g_dspInitialize.pGetAudioStreaming(samples, numSamples);
	}

	MixUCode(samples, numSamples);
}


