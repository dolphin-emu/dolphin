// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../Globals.h"
#include "UCodes.h"
#include "UCode_Zelda.h"
#include "UCode_Zelda_ADPCM.h"

#include "../main.h"
#include "Mixer.h"


class CResampler
{
public:
    CResampler(short* samples, int num_stereo_samples, int core_sample_rate)
        : m_mode(1)
        , m_queueSize(0)
    {
        int PV1l=0,PV2l=0,PV3l=0,PV4l=0;
        int acc=0;     

        while (num_stereo_samples) 
        {
            acc += core_sample_rate;
            while (num_stereo_samples && (acc >= 48000)) 
            {
                PV4l=PV3l;
                PV3l=PV2l;
                PV2l=PV1l;
                PV1l=*(samples++); //32bit processing
                num_stereo_samples--;
                acc-=48000;
            }

            // defaults to nearest
            s32 DataL = PV1l;

            if (m_mode == 1) { //linear
                DataL = PV1l + ((PV2l - PV1l)*acc)/48000;
            }
            else if (m_mode == 2) {//cubic
                s32 a0l = PV1l - PV2l - PV4l + PV3l;
                s32 a1l = PV4l - PV3l - a0l;
                s32 a2l = PV1l - PV4l;
                s32 a3l = PV2l;

                s32 t0l = ((a0l    )*acc)/48000;
                s32 t1l = ((t0l+a1l)*acc)/48000;
                s32 t2l = ((t1l+a2l)*acc)/48000;
                s32 t3l = ((t2l+a3l));

                DataL = t3l;
            }

            int l = DataL;
            if (l < -32767) l = -32767;
            if (l > 32767) l = 32767;
            sample_queue.push(l);
            m_queueSize += 1;
        }
    }

    FixedSizeQueue<s16, queue_maxlength> sample_queue;
    int m_queueSize;
    int m_mode;
};


void CUCode_Zelda::ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	// Perform byteswap
	for (int i = 0; i < (0x180 / 2); i++)
		((u16*)&PB)[i] = Common::swap16(memory[i]);

	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);
	PB.LoopStartPos = (PB.LoopStartPos << 16) | (PB.LoopStartPos >> 16);
	PB.Length = (PB.Length << 16) | (PB.Length >> 16);
	PB.StartAddr = (PB.StartAddr << 16) | (PB.StartAddr >> 16);
	PB.UnkAddr = (PB.UnkAddr << 16) | (PB.UnkAddr >> 16);
}

void CUCode_Zelda::WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);

	// Perform byteswap
	// Only the first 0x100 bytes are written back
	for (int i = 0; i < (0x100 / 2); i++)
		memory[i] = Common::swap16(((u16*)&PB)[i]);
}

void CUCode_Zelda::MixAddVoice_PCM16(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (((PB.RatioInt * 80) + PB.RatioFrac) << 4) & 0xFFFF0000;
	u64 ratio = (u64)(((_ratio / 80) << 16) * ratioFactor);
	u32 pos[2] = {0, 0};
	int i = 0;

	if (PB.KeyOff != 0)
		return;

	if (PB.NeedsReset)
	{
		PB.RemLength = PB.Length - PB.RestartPos;
		PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
		PB.ReachedEnd = 0;
	}

_lRestart:
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;

		if (PB.RepeatMode == 0)
		{
			PB.KeyOff = 1;
			PB.RemLength = 0;
			PB.CurAddr = PB.StartAddr + (PB.RestartPos << 1) + PB.Length;
			return;
		}
		else
		{
			PB.RestartPos = PB.LoopStartPos;
			PB.RemLength = PB.Length - PB.RestartPos;
			PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
			pos[1] = 0; pos[0] = 0;
		}
	}

	s16 *source;
	if (m_CRC == 0xD643001F)
		source = (s16*)(g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr) + PB.CurAddr);
	else
		source = (s16*)(g_dspInitialize.pGetARAMPointer() + PB.CurAddr);

	for (; i < _Size;)
	{
		s16 sample = Common::swap16(source[pos[1]]);

		_Buffer[i++] = (s32)sample;

		(*(u64*)&pos) += ratio;
		if ((pos[1] + ((PB.CurAddr - PB.StartAddr) >> 1)) >= PB.Length)
		{
			PB.ReachedEnd = 1;
			goto _lRestart;
		}
	}

	if (PB.RemLength < pos[1])
	{
		PB.RemLength = 0;
		PB.ReachedEnd = 1;
	}
	else
		PB.RemLength -= pos[1];

	PB.CurAddr += pos[1] << 1;
}

void CUCode_Zelda::MixAddVoice_AFC(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	// initialize "decoder" if the sample is played the first time
    if (PB.NeedsReset != 0)
    {
		// This is 0717_ReadOutPBStuff

		// increment 4fb

        // zelda: 
        // perhaps init or "has played before"
        PB.CurBlock = 0x00;
		PB.YN2 = 0x00;     // history1 
        PB.YN1 = 0x00;     // history2 

        // samplerate? length? num of samples? i dunno...
		// Likely length...
        PB.RemLength = PB.Length;

        // Copy ARAM addr from r to rw area.
        PB.CurAddr = PB.StartAddr;
    }

    if (PB.KeyOff != 0)  // 0747 early out... i dunno if this can happen because we filter it above
        return;

    u32 Addr			= PB.CurAddr;
    u32 NumberOfSamples = PB.RemLength;

	// round upwards how many samples we need to copy, 0759
    NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction
    u32 frac = NumberOfSamples & 0xF;

    u8 inBuffer[9];
    short outbuf[16];
    u32 sampleCount = 0;

	u8 *source;
	if (m_CRC == 0xD643001F)
		source = g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr);
	else
		source = g_dspInitialize.pGetARAMPointer();

	// It must be something like this:

	// The PB contains a small sample buffer of 0x4D decoded samples.
	// If it's empty or "used", decode to it.
	// Then, resample from this buffer to the output as you go. When it needs
	// wrapping, decode more.
	
    while (NumberOfSamples > 0)
    {
        for (int i = 0; i < 9; i++)    
        {
           // inBuffer[i] =  g_dspInitialize.pARAM_Read_U8(ARAMAddr);
			inBuffer[i] = source[Addr];
            Addr++;
        }

        AFCdecodebuffer(m_AFCCoefTable, (char*)inBuffer, outbuf, (short*)&PB.YN2, (short*)&PB.YN1, 9);
        CResampler Sampler(outbuf, 16, 48000);

        while (Sampler.m_queueSize > 0) 
        {
            int sample = Sampler.sample_queue.front();
            Sampler.sample_queue.pop();
            Sampler.m_queueSize -= 1;

           // templbuffer[sampleCount] += sample;
           // temprbuffer[sampleCount] += sample;
			_Buffer[sampleCount] = sample;
            sampleCount++;

            if (sampleCount > _Size)
                break;
        }

        if (sampleCount > _Size)
            break;

        NumberOfSamples--;
    }

    if (NumberOfSamples == 0)
    {
        PB.KeyOff = 1; // we are done ??
    }

    // write back
    NumberOfSamples = (NumberOfSamples << 4);    // missing fraction

    PB.CurAddr = Addr;
    PB.RemLength = NumberOfSamples;


    // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
    // just decrement them with the number of samples you have played
    // and incrrease the ARAM Offset in pTest[0x38], pTest[0x39]


    // end of block (Zelda 03b2)
    PB.NeedsReset = 0;
}

void CUCode_Zelda::MixAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	memset(m_TempBuffer, 0, _Size * sizeof(s32));

	if (PB.IsBlank)
	{
		s32 sample = (s32)(s16)PB.FixedSample;
		for (int i = 0; i < _Size; i++)
			m_TempBuffer[i] = sample;
	}
	else
	{
		switch (PB.Format)
		{
		case 0x0005:		// AFC / unknown
		case 0x0009:		// AFC / ADPCM
			MixAddVoice_AFC(PB, m_TempBuffer, _Size);
			break;

		case 0x0010:		// PCM16
			MixAddVoice_PCM16(PB, m_TempBuffer, _Size);
			break;
		}

		PB.NeedsReset = 0;
	}

	for (int i = 0; i < _Size; i++)
	{
		s32 left = _LeftBuffer[i] + m_TempBuffer[i];
		s32 right = _RightBuffer[i] + m_TempBuffer[i];

		if (left < -32768) left = -32768;
		if (left > 32767)  left = 32767;
		_LeftBuffer[i] = left;

		if (right < -32768) right = -32768;
		if (right > 32767)  right = 32767;
		_RightBuffer[i] = right;
	}
}
