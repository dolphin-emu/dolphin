// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "Common/MathUtil.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

//#define WIIMOTE_SPEAKER_DUMP
#ifdef WIIMOTE_SPEAKER_DUMP
#include <cstdlib>
#include <fstream>
#include "AudioCommon/WaveFile.h"
#include "Common/FileUtil.h"
#endif

namespace WiimoteEmu
{

// Yamaha ADPCM decoder code based on The ffmpeg Project (Copyright (s) 2001-2003)

static const s32 yamaha_difflookup[] = {
	1, 3, 5, 7, 9, 11, 13, 15,
	-1, -3, -5, -7, -9, -11, -13, -15
};

static const s32 yamaha_indexscale[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	230, 230, 230, 230, 307, 409, 512, 614
};

static s16 av_clip16(s32 a)
{
	if ((a+32768) & ~65535) return (a>>31) ^ 32767;
	else                    return a;
}

static s32 av_clip(s32 a, s32 amin, s32 amax)
{
	if      (a < amin) return amin;
	else if (a > amax) return amax;
	else               return a;
}

static s16 adpcm_yamaha_expand_nibble(ADPCMState& s, u8 nibble)
{
	s.predictor += (s.step * yamaha_difflookup[nibble]) / 8;
	s.predictor = av_clip16(s.predictor);
	s.step = (s.step * yamaha_indexscale[nibble]) >> 8;
	s.step = av_clip(s.step, 127, 24576);
	return s.predictor;
}

#ifdef WIIMOTE_SPEAKER_DUMP
std::ofstream ofile;
WaveFileWriter wav;

void stopdamnwav(){wav.Stop();ofile.close();}
#endif

void Wiimote::SpeakerData(wm_speaker_data* sd)
{
	if (!SConfig::GetInstance().m_WiimoteEnableSpeaker)
		return;
	if (m_reg_speaker.volume == 0 || m_reg_speaker.sample_rate == 0 || sd->length == 0)
		return;

	// TODO consider using static max size instead of new
	std::unique_ptr<s16[]> samples(new s16[sd->length * 2]);

	unsigned int sample_rate_dividend, sample_length;
	u8 volume_divisor;

	if (m_reg_speaker.format == 0x40)
	{
		// 8 bit PCM
		for (int i = 0; i < sd->length; ++i)
		{
			samples[i] = ((s16)(s8)sd->data[i]) << 8;
		}

		// Following details from http://wiibrew.org/wiki/Wiimote#Speaker
		sample_rate_dividend = 12000000;
		volume_divisor = 0xff;
		sample_length = (unsigned int)sd->length;
	}
	else if (m_reg_speaker.format == 0x00)
	{
		// 4 bit Yamaha ADPCM (same as dreamcast)
		for (int i = 0; i < sd->length; ++i)
		{
			samples[i * 2] = adpcm_yamaha_expand_nibble(m_adpcm_state, (sd->data[i] >> 4) & 0xf);
			samples[i * 2 + 1] = adpcm_yamaha_expand_nibble(m_adpcm_state, sd->data[i] & 0xf);
		}

		// Following details from http://wiibrew.org/wiki/Wiimote#Speaker
		sample_rate_dividend = 6000000;

		// Despite above source, max wiimote speaker observed in games seems to be 127, not 64
		volume_divisor = 0x7f;
		sample_length = (unsigned int)sd->length * 2;
	}
	else
	{
		ERROR_LOG(WII_IPC_WIIMOTE, "Unknown speaker format %x\n", m_reg_speaker.format);
		return;
	}

	// Speaker Pan using Constant Power Pan Law
	double pan = m_options->settings[4]->GetValue() / 1.27;
	double pan_prime = 3.14159265358979323846 * (pan + 1) / 4.0;
	unsigned int sample_rate = sample_rate_dividend / m_reg_speaker.sample_rate;
	float speaker_volume_ratio = (float)m_reg_speaker.volume / volume_divisor;

	unsigned int left_volume = (unsigned int)(256 * cos(pan_prime) * speaker_volume_ratio);
	unsigned int right_volume = (unsigned int)(256 * sin(pan_prime) * speaker_volume_ratio);

	left_volume = MathUtil::Clamp(left_volume, 0u, 256u);
	right_volume = MathUtil::Clamp(right_volume, 0u, 256u);

	g_sound_stream->GetMixer()->SetWiimoteSpeakerVolume(left_volume, right_volume);

	// ADPCM sample rate is thought to be x2.(3000 x2 = 6000).
	g_sound_stream->GetMixer()->PushWiimoteSpeakerSamples(samples.get(), sample_length, sample_rate * 2);

#ifdef WIIMOTE_SPEAKER_DUMP
	static int num = 0;

	if (num == 0)
	{
		File::Delete("rmtdump.wav");
		File::Delete("rmtdump.bin");
		atexit(stopdamnwav);
		OpenFStream(ofile, "rmtdump.bin", ofile.binary | ofile.out);
		wav.Start("rmtdump.wav", 6000/*Common::swap16(m_reg_speaker.sample_rate)*/);
	}
	wav.AddMonoSamples(samples.get(), sd->length*2);
	if (ofile.good())
	{
		for (int i = 0; i < sd->length; i++)
		{
			ofile << sd->data[i];
		}
	}
	num++;
#endif
}

}
