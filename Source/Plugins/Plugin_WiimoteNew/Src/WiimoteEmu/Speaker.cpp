
#include "WiimoteEmu.h"

#ifdef USE_WIIMOTE_EMU_SPEAKER

namespace WiimoteEmu
{

void Wiimote::SpeakerData(wm_speaker_data* sd)
{
	SoundBuffer sb;
	sb.samples = new s16[sd->length * 2];

	s16* s = sb.samples;
	const u8* const e = sd->data + sd->length;
	for ( const u8* i = sd->data; i<e; ++i )
	{
		*s++ = NGCADPCM::ADPDecodeSample(*i & 0x0F, sd->data[0] & 0x0F, &m_channel_status.hist1p, &m_channel_status.hist2p);
		*s++ = NGCADPCM::ADPDecodeSample(*i >> 4, sd->data[0] >> 4, &m_channel_status.hist1p, &m_channel_status.hist2p);
	}

	alGenBuffers(1, &sb.buffer);
	// TODO make this not always 3000
	alBufferData(sb.buffer, AL_FORMAT_MONO16, sb.samples, (sd->length * sizeof(short) * 2), 3360);
	// testing
	//alBufferData(sb.buffer, AL_FORMAT_MONO16, sb.samples, (sd->length * sizeof(short) * 2), 48000/m_reg_speaker->sample_rate);
	alSourceQueueBuffers(m_audio_source, 1, &sb.buffer);

	ALint state;
	alGetSourcei(m_audio_source, AL_SOURCE_STATE, &state);
	if (AL_PLAYING != state)
		alSourcePlay(m_audio_source);

	m_audio_buffers.push(sb);
}

}

#endif