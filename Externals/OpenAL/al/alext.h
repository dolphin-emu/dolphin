#ifndef _AL_ALEXT_H
#define _AL_ALEXT_H

#include <AL/al.h>
#include <AL/alc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* format base 0x10000 */
#define AL_FORMAT_IMA_ADPCM_MONO16_EXT            0x10000
#define AL_FORMAT_IMA_ADPCM_STEREO16_EXT          0x10001
#define AL_FORMAT_WAVE_EXT                        0x10002
#define AL_FORMAT_VORBIS_EXT                      0x10003

/* four point formats */
#define AL_FORMAT_QUAD8_LOKI                      0x10004
#define AL_FORMAT_QUAD16_LOKI                     0x10005

/**
 * token extensions, base 0x20000
 */

/* deprecated, use AL_GAIN */
#define AL_GAIN_LINEAR_LOKI                      0x20000

/*
 * types for special loaders.  This should be deprecated in favor
 * of the special format tags.
 */

typedef struct WaveFMT {
	ALushort encoding;
	ALushort channels;	/* 1 = mono, 2 = stereo */
	ALuint frequency;	/* One of 11025, 22050, or 44100 Hz */
	ALuint byterate;	/* Average bytes per second */
	ALushort blockalign;	/* Bytes per sample block */
	ALushort bitspersample;
} alWaveFMT_LOKI;

typedef struct _MS_ADPCM_decodestate {
	ALubyte hPredictor;
	ALushort iDelta;
	ALshort iSamp1;
	ALshort iSamp2;
} alMSADPCM_decodestate_LOKI;

typedef struct MS_ADPCM_decoder {
	alWaveFMT_LOKI wavefmt;
	ALushort wSamplesPerBlock;
	ALushort wNumCoef;
	ALshort aCoeff[7][2];
	/* * * */
	alMSADPCM_decodestate_LOKI state[2];
} alMSADPCM_state_LOKI;

typedef struct IMA_ADPCM_decodestate_s {
	ALint valprev;		/* Previous output value */
	ALbyte index;		/* Index into stepsize table */
} alIMAADPCM_decodestate_LOKI;

typedef struct IMA_ADPCM_decoder {
	alWaveFMT_LOKI wavefmt;
	ALushort wSamplesPerBlock;
	/* * * */
	alIMAADPCM_decodestate_LOKI state[2];
} alIMAADPCM_state_LOKI;

/**
 * Context creation extension tokens
 * base 0x200000
 */

/**
 * followed by ### of sources
 */
#define ALC_SOURCES_LOKI                         0x200000

/**
 * followed by ### of buffers
 */
#define ALC_BUFFERS_LOKI                         0x200001

/*
 *  Channel operations are probably a big no-no and destined
 *  for obsolesence.
 *
 *  base 0x300000
 */
#define	ALC_CHAN_MAIN_LOKI                       0x300000
#define	ALC_CHAN_PCM_LOKI                        0x300001
#define	ALC_CHAN_CD_LOKI                         0x300002

/* loki */

ALC_API ALfloat  ALC_APIENTRY alcGetAudioChannel_LOKI( ALuint channel );
ALC_API void     ALC_APIENTRY alcSetAudioChannel_LOKI( ALuint channel, ALfloat volume );
AL_API  void     AL_APIENTRY  alBombOnError_LOKI( void );
AL_API  void     AL_APIENTRY  alBufferi_LOKI( ALuint bid, ALenum param, ALint value );
AL_API  void     AL_APIENTRY  alBufferDataWithCallback_LOKI( ALuint bid, int ( *Callback ) ( ALuint, ALuint, ALshort *, ALenum, ALint, ALint ) );
AL_API  void     AL_APIENTRY  alBufferWriteData_LOKI( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq, ALenum internalFormat );
AL_API  void     AL_APIENTRY  alGenStreamingBuffers_LOKI( ALsizei n, ALuint *samples );
AL_API  ALsizei  AL_APIENTRY  alBufferAppendData_LOKI( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq );
AL_API  ALsizei  AL_APIENTRY  alBufferAppendWriteData_LOKI( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq, ALenum internalFormat );

/* binary compatibility */
AL_API  ALsizei  AL_APIENTRY alBufferAppendData( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq );

/*
 * Don't use these.  If you're reading this, you should remove these functions
 * and all other reverb functions.  Now.
 */
AL_API void AL_APIENTRY alReverbScale_LOKI(ALuint sid, ALfloat param);
AL_API void AL_APIENTRY alReverbDelay_LOKI(ALuint sid, ALfloat param);

/* custom loaders */

AL_API ALboolean AL_APIENTRY alutLoadVorbis_LOKI( ALuint bid, const ALvoid *data, ALint size );
AL_API ALboolean AL_APIENTRY alutLoadMP3_LOKI( ALuint bid, ALvoid *data, ALint size );

/* function pointers */

typedef ALfloat   ( ALC_APIENTRY *PFNALCGETAUDIOCHANNELPROC ) ( ALuint channel );
typedef void      ( ALC_APIENTRY *PFNALCSETAUDIOCHANNELPROC ) ( ALuint channel, ALfloat volume );
typedef void      ( AL_APIENTRY *PFNALBOMBONERRORPROC ) ( void );
typedef void      ( AL_APIENTRY *PFNALBUFFERIPROC ) ( ALuint bid, ALenum param, ALint value );
typedef void      ( AL_APIENTRY *PFNALBUFFERDATAWITHCALLBACKPROC ) ( ALuint bid, int ( *Callback ) ( ALuint, ALuint, ALshort *, ALenum, ALint, ALint ) );
typedef void      ( AL_APIENTRY *PFNALBUFFERWRITEDATAPROC ) ( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq, ALenum internalFormat );
typedef void      ( AL_APIENTRY *PFNALGENSTREAMINGBUFFERSPROC ) ( ALsizei n, ALuint *samples );
typedef ALsizei   ( AL_APIENTRY *PFNALBUFFERAPPENDDATAPROC ) ( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq );
typedef ALsizei   ( AL_APIENTRY *PFNALBUFFERAPPENDWRITEDATAPROC ) ( ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq, ALenum internalFormat );

typedef ALboolean ( AL_APIENTRY *PFNALUTLOADVORBISPROC ) ( ALuint bid, ALvoid *data, ALint size );
typedef ALboolean ( AL_APIENTRY *PFNALUTLOADRAW_ADPCMDATAPROC ) ( ALuint bid, ALvoid *data, ALuint size, ALuint freq, ALenum format );
typedef ALboolean ( AL_APIENTRY *ALUTLOADIMA_ADPCMDATAPROC ) ( ALuint bid, ALvoid *data, ALuint size, alIMAADPCM_state_LOKI *ias );
typedef ALboolean ( AL_APIENTRY *ALUTLOADMS_ADPCMDATAPROC ) ( ALuint bid, void *data, int size, alMSADPCM_state_LOKI *mss );

typedef void      ( AL_APIENTRY *PFNALREVERBSCALEPROC ) ( ALuint sid, ALfloat param );
typedef void      ( AL_APIENTRY *PFNALREVERBDELAYPROC ) ( ALuint sid, ALfloat param );

#ifdef __cplusplus
}
#endif

#endif				/* _AL_ALEXT_H */
