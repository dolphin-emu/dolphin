#ifndef AL_ALEXT_H
#define AL_ALEXT_H

/* NOLINTBEGIN */
#include <stddef.h>
/* Define int64 and uint64 types */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) ||             \
    (defined(__cplusplus) && __cplusplus >= 201103L)
#include <stdint.h>
typedef int64_t _alsoft_int64_t;
typedef uint64_t _alsoft_uint64_t;
#elif defined(_WIN32)
typedef __int64 _alsoft_int64_t;
typedef unsigned __int64 _alsoft_uint64_t;
#else
/* Fallback if nothing above works */
#include <stdint.h>
typedef int64_t _alsoft_int64_t;
typedef uint64_t _alsoft_uint64_t;
#endif

#include "alc.h"
#include "al.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _GUID;

#ifndef AL_LOKI_IMA_ADPCM_format
#define AL_LOKI_IMA_ADPCM_format 1
#define AL_FORMAT_IMA_ADPCM_MONO16_EXT           0x10000
#define AL_FORMAT_IMA_ADPCM_STEREO16_EXT         0x10001
#endif

#ifndef AL_LOKI_WAVE_format
#define AL_LOKI_WAVE_format 1
#define AL_FORMAT_WAVE_EXT                       0x10002
#endif

#ifndef AL_EXT_vorbis
#define AL_EXT_vorbis 1
#define AL_FORMAT_VORBIS_EXT                     0x10003
#endif

#ifndef AL_LOKI_quadriphonic
#define AL_LOKI_quadriphonic 1
#define AL_FORMAT_QUAD8_LOKI                     0x10004
#define AL_FORMAT_QUAD16_LOKI                    0x10005
#endif

#ifndef AL_EXT_float32
#define AL_EXT_float32 1
#define AL_FORMAT_MONO_FLOAT32                   0x10010
#define AL_FORMAT_STEREO_FLOAT32                 0x10011
#endif

#ifndef AL_EXT_double
#define AL_EXT_double 1
#define AL_FORMAT_MONO_DOUBLE_EXT                0x10012
#define AL_FORMAT_STEREO_DOUBLE_EXT              0x10013
#endif

#ifndef AL_EXT_MULAW
#define AL_EXT_MULAW 1
#define AL_FORMAT_MONO_MULAW_EXT                 0x10014
#define AL_FORMAT_STEREO_MULAW_EXT               0x10015
#endif

#ifndef AL_EXT_ALAW
#define AL_EXT_ALAW 1
#define AL_FORMAT_MONO_ALAW_EXT                  0x10016
#define AL_FORMAT_STEREO_ALAW_EXT                0x10017
#endif

#ifndef ALC_LOKI_audio_channel
#define ALC_LOKI_audio_channel 1
#define ALC_CHAN_MAIN_LOKI                       0x500001
#define ALC_CHAN_PCM_LOKI                        0x500002
#define ALC_CHAN_CD_LOKI                         0x500003
#endif

#ifndef AL_EXT_MCFORMATS
#define AL_EXT_MCFORMATS 1
/* Provides support for surround sound buffer formats with 8, 16, and 32-bit
 * samples.
 *
 * QUAD8: Unsigned 8-bit, Quadraphonic (Front Left, Front Right, Rear Left,
 *        Rear Right).
 * QUAD16: Signed 16-bit, Quadraphonic.
 * QUAD32: 32-bit float, Quadraphonic.
 * REAR8: Unsigned 8-bit, Rear Stereo (Rear Left, Rear Right).
 * REAR16: Signed 16-bit, Rear Stereo.
 * REAR32: 32-bit float, Rear Stereo.
 * 51CHN8: Unsigned 8-bit, 5.1 Surround (Front Left, Front Right, Front Center,
 *         LFE, Side Left, Side Right). Note that some audio systems may label
 *         5.1's Side channels as Rear or Surround; they are equivalent for the
 *         purposes of this extension.
 * 51CHN16: Signed 16-bit, 5.1 Surround.
 * 51CHN32: 32-bit float, 5.1 Surround.
 * 61CHN8: Unsigned 8-bit, 6.1 Surround (Front Left, Front Right, Front Center,
 *         LFE, Rear Center, Side Left, Side Right).
 * 61CHN16: Signed 16-bit, 6.1 Surround.
 * 61CHN32: 32-bit float, 6.1 Surround.
 * 71CHN8: Unsigned 8-bit, 7.1 Surround (Front Left, Front Right, Front Center,
 *         LFE, Rear Left, Rear Right, Side Left, Side Right).
 * 71CHN16: Signed 16-bit, 7.1 Surround.
 * 71CHN32: 32-bit float, 7.1 Surround.
 */
#define AL_FORMAT_QUAD8                          0x1204
#define AL_FORMAT_QUAD16                         0x1205
#define AL_FORMAT_QUAD32                         0x1206
#define AL_FORMAT_REAR8                          0x1207
#define AL_FORMAT_REAR16                         0x1208
#define AL_FORMAT_REAR32                         0x1209
#define AL_FORMAT_51CHN8                         0x120A
#define AL_FORMAT_51CHN16                        0x120B
#define AL_FORMAT_51CHN32                        0x120C
#define AL_FORMAT_61CHN8                         0x120D
#define AL_FORMAT_61CHN16                        0x120E
#define AL_FORMAT_61CHN32                        0x120F
#define AL_FORMAT_71CHN8                         0x1210
#define AL_FORMAT_71CHN16                        0x1211
#define AL_FORMAT_71CHN32                        0x1212
#endif

#ifndef AL_EXT_MULAW_MCFORMATS
#define AL_EXT_MULAW_MCFORMATS 1
#define AL_FORMAT_MONO_MULAW                     0x10014
#define AL_FORMAT_STEREO_MULAW                   0x10015
#define AL_FORMAT_QUAD_MULAW                     0x10021
#define AL_FORMAT_REAR_MULAW                     0x10022
#define AL_FORMAT_51CHN_MULAW                    0x10023
#define AL_FORMAT_61CHN_MULAW                    0x10024
#define AL_FORMAT_71CHN_MULAW                    0x10025
#endif

#ifndef AL_EXT_IMA4
#define AL_EXT_IMA4 1
#define AL_FORMAT_MONO_IMA4                      0x1300
#define AL_FORMAT_STEREO_IMA4                    0x1301
#endif

#ifndef AL_EXT_STATIC_BUFFER
#define AL_EXT_STATIC_BUFFER 1
typedef void (AL_APIENTRY*PFNALBUFFERDATASTATICPROC)(const ALuint,ALenum,ALvoid*,ALsizei,ALsizei) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
void AL_APIENTRY alBufferDataStatic(const ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_EXT_EFX
#define ALC_EXT_EFX 1
#include "efx.h"
#endif

#ifndef ALC_EXT_disconnect
#define ALC_EXT_disconnect 1
#define ALC_CONNECTED                            0x313
#endif

#ifndef ALC_EXT_thread_local_context
#define ALC_EXT_thread_local_context 1
typedef ALCboolean  (ALC_APIENTRY*PFNALCSETTHREADCONTEXTPROC)(ALCcontext *context) ALC_API_NOEXCEPT17;
typedef ALCcontext* (ALC_APIENTRY*PFNALCGETTHREADCONTEXTPROC)(void) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALC_API ALCboolean  ALC_APIENTRY alcSetThreadContext(ALCcontext *context) ALC_API_NOEXCEPT;
ALC_API ALCcontext* ALC_APIENTRY alcGetThreadContext(void) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_EXT_source_distance_model
#define AL_EXT_source_distance_model 1
#define AL_SOURCE_DISTANCE_MODEL                 0x200
#endif

#ifndef AL_SOFT_buffer_sub_data
#define AL_SOFT_buffer_sub_data 1
#define AL_BYTE_RW_OFFSETS_SOFT                  0x1031
#define AL_SAMPLE_RW_OFFSETS_SOFT                0x1032
typedef void (AL_APIENTRY*PFNALBUFFERSUBDATASOFTPROC)(ALuint,ALenum,const ALvoid*,ALsizei,ALsizei) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alBufferSubDataSOFT(ALuint buffer,ALenum format,const ALvoid *data,ALsizei offset,ALsizei length) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_loop_points
#define AL_SOFT_loop_points 1
#define AL_LOOP_POINTS_SOFT                      0x2015
#endif

#ifndef AL_EXT_FOLDBACK
#define AL_EXT_FOLDBACK 1
#define AL_EXT_FOLDBACK_NAME                     "AL_EXT_FOLDBACK"
#define AL_FOLDBACK_EVENT_BLOCK                  0x4112
#define AL_FOLDBACK_EVENT_START                  0x4111
#define AL_FOLDBACK_EVENT_STOP                   0x4113
#define AL_FOLDBACK_MODE_MONO                    0x4101
#define AL_FOLDBACK_MODE_STEREO                  0x4102
typedef void (AL_APIENTRY*LPALFOLDBACKCALLBACK)(ALenum,ALsizei) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALREQUESTFOLDBACKSTART)(ALenum,ALsizei,ALsizei,ALfloat*,LPALFOLDBACKCALLBACK) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALREQUESTFOLDBACKSTOP)(void) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alRequestFoldbackStart(ALenum mode,ALsizei count,ALsizei length,ALfloat *mem,LPALFOLDBACKCALLBACK callback) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alRequestFoldbackStop(void) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_EXT_DEDICATED
#define ALC_EXT_DEDICATED 1
#define AL_DEDICATED_GAIN                        0x0001
#define AL_EFFECT_DEDICATED_DIALOGUE             0x9001
#define AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT 0x9000
#endif

#ifndef AL_SOFT_buffer_samples
#define AL_SOFT_buffer_samples 1
/* Channel configurations */
#define AL_MONO_SOFT                             0x1500
#define AL_STEREO_SOFT                           0x1501
#define AL_REAR_SOFT                             0x1502
#define AL_QUAD_SOFT                             0x1503
#define AL_5POINT1_SOFT                          0x1504
#define AL_6POINT1_SOFT                          0x1505
#define AL_7POINT1_SOFT                          0x1506

/* Sample types */
#define AL_BYTE_SOFT                             0x1400
#define AL_UNSIGNED_BYTE_SOFT                    0x1401
#define AL_SHORT_SOFT                            0x1402
#define AL_UNSIGNED_SHORT_SOFT                   0x1403
#define AL_INT_SOFT                              0x1404
#define AL_UNSIGNED_INT_SOFT                     0x1405
#define AL_FLOAT_SOFT                            0x1406
#define AL_DOUBLE_SOFT                           0x1407
#define AL_BYTE3_SOFT                            0x1408
#define AL_UNSIGNED_BYTE3_SOFT                   0x1409

/* Storage formats */
#define AL_MONO8_SOFT                            0x1100
#define AL_MONO16_SOFT                           0x1101
#define AL_MONO32F_SOFT                          0x10010
#define AL_STEREO8_SOFT                          0x1102
#define AL_STEREO16_SOFT                         0x1103
#define AL_STEREO32F_SOFT                        0x10011
#define AL_QUAD8_SOFT                            0x1204
#define AL_QUAD16_SOFT                           0x1205
#define AL_QUAD32F_SOFT                          0x1206
#define AL_REAR8_SOFT                            0x1207
#define AL_REAR16_SOFT                           0x1208
#define AL_REAR32F_SOFT                          0x1209
#define AL_5POINT1_8_SOFT                        0x120A
#define AL_5POINT1_16_SOFT                       0x120B
#define AL_5POINT1_32F_SOFT                      0x120C
#define AL_6POINT1_8_SOFT                        0x120D
#define AL_6POINT1_16_SOFT                       0x120E
#define AL_6POINT1_32F_SOFT                      0x120F
#define AL_7POINT1_8_SOFT                        0x1210
#define AL_7POINT1_16_SOFT                       0x1211
#define AL_7POINT1_32F_SOFT                      0x1212

/* Buffer attributes */
#define AL_INTERNAL_FORMAT_SOFT                  0x2008
#define AL_BYTE_LENGTH_SOFT                      0x2009
#define AL_SAMPLE_LENGTH_SOFT                    0x200A
#define AL_SEC_LENGTH_SOFT                       0x200B

typedef void (AL_APIENTRY*LPALBUFFERSAMPLESSOFT)(ALuint,ALuint,ALenum,ALsizei,ALenum,ALenum,const ALvoid*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALBUFFERSUBSAMPLESSOFT)(ALuint,ALsizei,ALsizei,ALenum,ALenum,const ALvoid*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETBUFFERSAMPLESSOFT)(ALuint,ALsizei,ALsizei,ALenum,ALenum,ALvoid*) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY*LPALISBUFFERFORMATSUPPORTEDSOFT)(ALenum) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alBufferSamplesSOFT(ALuint buffer, ALuint samplerate, ALenum internalformat, ALsizei samples, ALenum channels, ALenum type, const ALvoid *data) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alBufferSubSamplesSOFT(ALuint buffer, ALsizei offset, ALsizei samples, ALenum channels, ALenum type, const ALvoid *data) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetBufferSamplesSOFT(ALuint buffer, ALsizei offset, ALsizei samples, ALenum channels, ALenum type, ALvoid *data) AL_API_NOEXCEPT;
AL_API ALboolean AL_APIENTRY alIsBufferFormatSupportedSOFT(ALenum format) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_direct_channels
#define AL_SOFT_direct_channels 1
#define AL_DIRECT_CHANNELS_SOFT                  0x1033
#endif

#ifndef ALC_SOFT_loopback
#define ALC_SOFT_loopback 1
#define ALC_FORMAT_CHANNELS_SOFT                 0x1990
#define ALC_FORMAT_TYPE_SOFT                     0x1991

/* Sample types */
#define ALC_BYTE_SOFT                            0x1400
#define ALC_UNSIGNED_BYTE_SOFT                   0x1401
#define ALC_SHORT_SOFT                           0x1402
#define ALC_UNSIGNED_SHORT_SOFT                  0x1403
#define ALC_INT_SOFT                             0x1404
#define ALC_UNSIGNED_INT_SOFT                    0x1405
#define ALC_FLOAT_SOFT                           0x1406

/* Channel configurations */
#define ALC_MONO_SOFT                            0x1500
#define ALC_STEREO_SOFT                          0x1501
#define ALC_QUAD_SOFT                            0x1503
#define ALC_5POINT1_SOFT                         0x1504
#define ALC_6POINT1_SOFT                         0x1505
#define ALC_7POINT1_SOFT                         0x1506

typedef ALCdevice* (ALC_APIENTRY*LPALCLOOPBACKOPENDEVICESOFT)(const ALCchar*) ALC_API_NOEXCEPT17;
typedef ALCboolean (ALC_APIENTRY*LPALCISRENDERFORMATSUPPORTEDSOFT)(ALCdevice*,ALCsizei,ALCenum,ALCenum) ALC_API_NOEXCEPT17;
typedef void (ALC_APIENTRY*LPALCRENDERSAMPLESSOFT)(ALCdevice*,ALCvoid*,ALCsizei) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALC_API ALCdevice* ALC_APIENTRY alcLoopbackOpenDeviceSOFT(const ALCchar *deviceName) AL_API_NOEXCEPT;
ALC_API ALCboolean ALC_APIENTRY alcIsRenderFormatSupportedSOFT(ALCdevice *device, ALCsizei freq, ALCenum channels, ALCenum type) AL_API_NOEXCEPT;
ALC_API void ALC_APIENTRY alcRenderSamplesSOFT(ALCdevice *device, ALCvoid *buffer, ALCsizei samples) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_EXT_STEREO_ANGLES
#define AL_EXT_STEREO_ANGLES 1
#define AL_STEREO_ANGLES                         0x1030
#endif

#ifndef AL_EXT_SOURCE_RADIUS
#define AL_EXT_SOURCE_RADIUS 1
#define AL_SOURCE_RADIUS                         0x1031
#endif

#ifndef AL_SOFT_source_latency
#define AL_SOFT_source_latency 1
#define AL_SAMPLE_OFFSET_LATENCY_SOFT            0x1200
#define AL_SEC_OFFSET_LATENCY_SOFT               0x1201
typedef _alsoft_int64_t ALint64SOFT;
typedef _alsoft_uint64_t ALuint64SOFT;
typedef void (AL_APIENTRY*LPALSOURCEDSOFT)(ALuint,ALenum,ALdouble) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCE3DSOFT)(ALuint,ALenum,ALdouble,ALdouble,ALdouble) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCEDVSOFT)(ALuint,ALenum,const ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCEDSOFT)(ALuint,ALenum,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCE3DSOFT)(ALuint,ALenum,ALdouble*,ALdouble*,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCEDVSOFT)(ALuint,ALenum,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCEI64SOFT)(ALuint,ALenum,ALint64SOFT) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCE3I64SOFT)(ALuint,ALenum,ALint64SOFT,ALint64SOFT,ALint64SOFT) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCEI64VSOFT)(ALuint,ALenum,const ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCEI64SOFT)(ALuint,ALenum,ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCE3I64SOFT)(ALuint,ALenum,ALint64SOFT*,ALint64SOFT*,ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETSOURCEI64VSOFT)(ALuint,ALenum,ALint64SOFT*) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alSourcedSOFT(ALuint source, ALenum param, ALdouble value) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alSource3dSOFT(ALuint source, ALenum param, ALdouble value1, ALdouble value2, ALdouble value3) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alSourcedvSOFT(ALuint source, ALenum param, const ALdouble *values) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSourcedSOFT(ALuint source, ALenum param, ALdouble *value) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSource3dSOFT(ALuint source, ALenum param, ALdouble *value1, ALdouble *value2, ALdouble *value3) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSourcedvSOFT(ALuint source, ALenum param, ALdouble *values) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT value) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT value1, ALint64SOFT value2, ALint64SOFT value3) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alSourcei64vSOFT(ALuint source, ALenum param, const ALint64SOFT *values) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT *value) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT *value1, ALint64SOFT *value2, ALint64SOFT *value3) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetSourcei64vSOFT(ALuint source, ALenum param, ALint64SOFT *values) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_EXT_DEFAULT_FILTER_ORDER
#define ALC_EXT_DEFAULT_FILTER_ORDER 1
#define ALC_DEFAULT_FILTER_ORDER                 0x1100
#endif

#ifndef AL_SOFT_deferred_updates
#define AL_SOFT_deferred_updates 1
#define AL_DEFERRED_UPDATES_SOFT                 0xC002
typedef void (AL_APIENTRY*LPALDEFERUPDATESSOFT)(void) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALPROCESSUPDATESSOFT)(void) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alDeferUpdatesSOFT(void) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alProcessUpdatesSOFT(void) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_block_alignment
#define AL_SOFT_block_alignment 1
#define AL_UNPACK_BLOCK_ALIGNMENT_SOFT           0x200C
#define AL_PACK_BLOCK_ALIGNMENT_SOFT             0x200D
#endif

#ifndef AL_SOFT_MSADPCM
#define AL_SOFT_MSADPCM 1
#define AL_FORMAT_MONO_MSADPCM_SOFT              0x1302
#define AL_FORMAT_STEREO_MSADPCM_SOFT            0x1303
#endif

#ifndef AL_SOFT_source_length
#define AL_SOFT_source_length 1
/*#define AL_BYTE_LENGTH_SOFT                      0x2009*/
/*#define AL_SAMPLE_LENGTH_SOFT                    0x200A*/
/*#define AL_SEC_LENGTH_SOFT                       0x200B*/
#endif

#ifndef AL_SOFT_buffer_length_query
#define AL_SOFT_buffer_length_query 1
/*#define AL_BYTE_LENGTH_SOFT                      0x2009*/
/*#define AL_SAMPLE_LENGTH_SOFT                    0x200A*/
/*#define AL_SEC_LENGTH_SOFT                       0x200B*/
#endif

#ifndef ALC_SOFT_pause_device
#define ALC_SOFT_pause_device 1
typedef void (ALC_APIENTRY*LPALCDEVICEPAUSESOFT)(ALCdevice *device) ALC_API_NOEXCEPT17;
typedef void (ALC_APIENTRY*LPALCDEVICERESUMESOFT)(ALCdevice *device) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALC_API void ALC_APIENTRY alcDevicePauseSOFT(ALCdevice *device) ALC_API_NOEXCEPT;
ALC_API void ALC_APIENTRY alcDeviceResumeSOFT(ALCdevice *device) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_EXT_BFORMAT
#define AL_EXT_BFORMAT 1
/* Provides support for B-Format ambisonic buffers (first-order, FuMa scaling
 * and layout).
 *
 * BFORMAT2D_8: Unsigned 8-bit, 3-channel non-periphonic (WXY).
 * BFORMAT2D_16: Signed 16-bit, 3-channel non-periphonic (WXY).
 * BFORMAT2D_FLOAT32: 32-bit float, 3-channel non-periphonic (WXY).
 * BFORMAT3D_8: Unsigned 8-bit, 4-channel periphonic (WXYZ).
 * BFORMAT3D_16: Signed 16-bit, 4-channel periphonic (WXYZ).
 * BFORMAT3D_FLOAT32: 32-bit float, 4-channel periphonic (WXYZ).
 */
#define AL_FORMAT_BFORMAT2D_8                    0x20021
#define AL_FORMAT_BFORMAT2D_16                   0x20022
#define AL_FORMAT_BFORMAT2D_FLOAT32              0x20023
#define AL_FORMAT_BFORMAT3D_8                    0x20031
#define AL_FORMAT_BFORMAT3D_16                   0x20032
#define AL_FORMAT_BFORMAT3D_FLOAT32              0x20033
#endif

#ifndef AL_EXT_MULAW_BFORMAT
#define AL_EXT_MULAW_BFORMAT 1
#define AL_FORMAT_BFORMAT2D_MULAW                0x10031
#define AL_FORMAT_BFORMAT3D_MULAW                0x10032
#endif

#ifndef ALC_SOFT_HRTF
#define ALC_SOFT_HRTF 1
#define ALC_HRTF_SOFT                            0x1992
#define ALC_DONT_CARE_SOFT                       0x0002
#define ALC_HRTF_STATUS_SOFT                     0x1993
#define ALC_HRTF_DISABLED_SOFT                   0x0000
#define ALC_HRTF_ENABLED_SOFT                    0x0001
#define ALC_HRTF_DENIED_SOFT                     0x0002
#define ALC_HRTF_REQUIRED_SOFT                   0x0003
#define ALC_HRTF_HEADPHONES_DETECTED_SOFT        0x0004
#define ALC_HRTF_UNSUPPORTED_FORMAT_SOFT         0x0005
#define ALC_NUM_HRTF_SPECIFIERS_SOFT             0x1994
#define ALC_HRTF_SPECIFIER_SOFT                  0x1995
#define ALC_HRTF_ID_SOFT                         0x1996
typedef const ALCchar* (ALC_APIENTRY*LPALCGETSTRINGISOFT)(ALCdevice *device, ALCenum paramName, ALCsizei index) ALC_API_NOEXCEPT17;
typedef ALCboolean (ALC_APIENTRY*LPALCRESETDEVICESOFT)(ALCdevice *device, const ALCint *attribs) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALC_API const ALCchar* ALC_APIENTRY alcGetStringiSOFT(ALCdevice *device, ALCenum paramName, ALCsizei index) ALC_API_NOEXCEPT;
ALC_API ALCboolean ALC_APIENTRY alcResetDeviceSOFT(ALCdevice *device, const ALCint *attribs) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_gain_clamp_ex
#define AL_SOFT_gain_clamp_ex 1
#define AL_GAIN_LIMIT_SOFT                       0x200E
#endif

#ifndef AL_SOFT_source_resampler
#define AL_SOFT_source_resampler
#define AL_NUM_RESAMPLERS_SOFT                   0x1210
#define AL_DEFAULT_RESAMPLER_SOFT                0x1211
#define AL_SOURCE_RESAMPLER_SOFT                 0x1212
#define AL_RESAMPLER_NAME_SOFT                   0x1213
typedef const ALchar* (AL_APIENTRY*LPALGETSTRINGISOFT)(ALenum pname, ALsizei index) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API const ALchar* AL_APIENTRY alGetStringiSOFT(ALenum pname, ALsizei index) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_source_spatialize
#define AL_SOFT_source_spatialize
#define AL_SOURCE_SPATIALIZE_SOFT                0x1214
#define AL_AUTO_SOFT                             0x0002
#endif

#ifndef ALC_SOFT_output_limiter
#define ALC_SOFT_output_limiter
#define ALC_OUTPUT_LIMITER_SOFT                  0x199A
#endif

#ifndef ALC_SOFT_device_clock
#define ALC_SOFT_device_clock 1
typedef _alsoft_int64_t ALCint64SOFT;
typedef _alsoft_uint64_t ALCuint64SOFT;
#define ALC_DEVICE_CLOCK_SOFT                    0x1600
#define ALC_DEVICE_LATENCY_SOFT                  0x1601
#define ALC_DEVICE_CLOCK_LATENCY_SOFT            0x1602
#define AL_SAMPLE_OFFSET_CLOCK_SOFT              0x1202
#define AL_SEC_OFFSET_CLOCK_SOFT                 0x1203
typedef void (ALC_APIENTRY*LPALCGETINTEGER64VSOFT)(ALCdevice *device, ALCenum pname, ALsizei size, ALCint64SOFT *values) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALC_API void ALC_APIENTRY alcGetInteger64vSOFT(ALCdevice *device, ALCenum pname, ALsizei size, ALCint64SOFT *values) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_direct_channels_remix
#define AL_SOFT_direct_channels_remix 1
#define AL_DROP_UNMATCHED_SOFT                   0x0001
#define AL_REMIX_UNMATCHED_SOFT                  0x0002
#endif

#ifndef AL_SOFT_bformat_ex
#define AL_SOFT_bformat_ex 1
#define AL_AMBISONIC_LAYOUT_SOFT                 0x1997
#define AL_AMBISONIC_SCALING_SOFT                0x1998

/* Ambisonic layouts */
#define AL_FUMA_SOFT                             0x0000
#define AL_ACN_SOFT                              0x0001

/* Ambisonic scalings (normalization) */
/*#define AL_FUMA_SOFT*/
#define AL_SN3D_SOFT                             0x0001
#define AL_N3D_SOFT                              0x0002
#endif

#ifndef ALC_SOFT_loopback_bformat
#define ALC_SOFT_loopback_bformat 1
#define ALC_AMBISONIC_LAYOUT_SOFT                0x1997
#define ALC_AMBISONIC_SCALING_SOFT               0x1998
#define ALC_AMBISONIC_ORDER_SOFT                 0x1999
#define ALC_MAX_AMBISONIC_ORDER_SOFT             0x199B

#define ALC_BFORMAT3D_SOFT                       0x1507

/* Ambisonic layouts */
#define ALC_FUMA_SOFT                            0x0000
#define ALC_ACN_SOFT                             0x0001

/* Ambisonic scalings (normalization) */
/*#define ALC_FUMA_SOFT*/
#define ALC_SN3D_SOFT                            0x0001
#define ALC_N3D_SOFT                             0x0002
#endif

#ifndef AL_SOFT_effect_target
#define AL_SOFT_effect_target
#define AL_EFFECTSLOT_TARGET_SOFT                0x199C
#endif

#ifndef AL_SOFT_events
#define AL_SOFT_events 1
#define AL_EVENT_CALLBACK_FUNCTION_SOFT          0x19A2
#define AL_EVENT_CALLBACK_USER_PARAM_SOFT        0x19A3
#define AL_EVENT_TYPE_BUFFER_COMPLETED_SOFT      0x19A4
#define AL_EVENT_TYPE_SOURCE_STATE_CHANGED_SOFT  0x19A5
#define AL_EVENT_TYPE_DISCONNECTED_SOFT          0x19A6
typedef void (AL_APIENTRY*ALEVENTPROCSOFT)(ALenum eventType, ALuint object, ALuint param,
    ALsizei length, const ALchar *message, void *userParam) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALEVENTCONTROLSOFT)(ALsizei count, const ALenum *types, ALboolean enable) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALEVENTCALLBACKSOFT)(ALEVENTPROCSOFT callback, void *userParam) AL_API_NOEXCEPT17;
typedef void* (AL_APIENTRY*LPALGETPOINTERSOFT)(ALenum pname) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETPOINTERVSOFT)(ALenum pname, void **values) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alEventControlSOFT(ALsizei count, const ALenum *types, ALboolean enable) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alEventCallbackSOFT(ALEVENTPROCSOFT callback, void *userParam) AL_API_NOEXCEPT;
AL_API void* AL_APIENTRY alGetPointerSOFT(ALenum pname) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetPointervSOFT(ALenum pname, void **values) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_SOFT_reopen_device
#define ALC_SOFT_reopen_device
typedef ALCboolean (ALC_APIENTRY*LPALCREOPENDEVICESOFT)(ALCdevice *device,
    const ALCchar *deviceName, const ALCint *attribs) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALCboolean ALC_APIENTRY alcReopenDeviceSOFT(ALCdevice *device, const ALCchar *deviceName,
    const ALCint *attribs) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_callback_buffer
#define AL_SOFT_callback_buffer
#define AL_BUFFER_CALLBACK_FUNCTION_SOFT         0x19A0
#define AL_BUFFER_CALLBACK_USER_PARAM_SOFT       0x19A1
typedef ALsizei (AL_APIENTRY*ALBUFFERCALLBACKTYPESOFT)(ALvoid *userptr, ALvoid *sampledata, ALsizei numbytes) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALBUFFERCALLBACKSOFT)(ALuint buffer, ALenum format, ALsizei freq, ALBUFFERCALLBACKTYPESOFT callback, ALvoid *userptr) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETBUFFERPTRSOFT)(ALuint buffer, ALenum param, ALvoid **value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETBUFFER3PTRSOFT)(ALuint buffer, ALenum param, ALvoid **value1, ALvoid **value2, ALvoid **value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETBUFFERPTRVSOFT)(ALuint buffer, ALenum param, ALvoid **values) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
AL_API void AL_APIENTRY alBufferCallbackSOFT(ALuint buffer, ALenum format, ALsizei freq, ALBUFFERCALLBACKTYPESOFT callback, ALvoid *userptr) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetBufferPtrSOFT(ALuint buffer, ALenum param, ALvoid **ptr) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetBuffer3PtrSOFT(ALuint buffer, ALenum param, ALvoid **ptr0, ALvoid **ptr1, ALvoid **ptr2) AL_API_NOEXCEPT;
AL_API void AL_APIENTRY alGetBufferPtrvSOFT(ALuint buffer, ALenum param, ALvoid **ptr) AL_API_NOEXCEPT;
#endif
#endif

#ifndef AL_SOFT_UHJ
#define AL_SOFT_UHJ
#define AL_FORMAT_UHJ2CHN8_SOFT                  0x19A2
#define AL_FORMAT_UHJ2CHN16_SOFT                 0x19A3
#define AL_FORMAT_UHJ2CHN_FLOAT32_SOFT           0x19A4
#define AL_FORMAT_UHJ3CHN8_SOFT                  0x19A5
#define AL_FORMAT_UHJ3CHN16_SOFT                 0x19A6
#define AL_FORMAT_UHJ3CHN_FLOAT32_SOFT           0x19A7
#define AL_FORMAT_UHJ4CHN8_SOFT                  0x19A8
#define AL_FORMAT_UHJ4CHN16_SOFT                 0x19A9
#define AL_FORMAT_UHJ4CHN_FLOAT32_SOFT           0x19AA

#define AL_STEREO_MODE_SOFT                      0x19B0
#define AL_NORMAL_SOFT                           0x0000
#define AL_SUPER_STEREO_SOFT                     0x0001
#define AL_SUPER_STEREO_WIDTH_SOFT               0x19B1
#endif

#ifndef AL_SOFT_UHJ_ex
#define AL_SOFT_UHJ_ex
#define AL_FORMAT_UHJ2CHN_MULAW_SOFT             0x19B3
#define AL_FORMAT_UHJ2CHN_ALAW_SOFT              0x19B4
#define AL_FORMAT_UHJ2CHN_IMA4_SOFT              0x19B5
#define AL_FORMAT_UHJ2CHN_MSADPCM_SOFT           0x19B6
#define AL_FORMAT_UHJ3CHN_MULAW_SOFT             0x19B7
#define AL_FORMAT_UHJ3CHN_ALAW_SOFT              0x19B8
#define AL_FORMAT_UHJ4CHN_MULAW_SOFT             0x19B9
#define AL_FORMAT_UHJ4CHN_ALAW_SOFT              0x19BA
#endif

#ifndef ALC_SOFT_output_mode
#define ALC_SOFT_output_mode
#define ALC_OUTPUT_MODE_SOFT                     0x19AC
#define ALC_ANY_SOFT                             0x19AD
/*#define ALC_MONO_SOFT                            0x1500*/
/*#define ALC_STEREO_SOFT                          0x1501*/
#define ALC_STEREO_BASIC_SOFT                    0x19AE
#define ALC_STEREO_UHJ_SOFT                      0x19AF
#define ALC_STEREO_HRTF_SOFT                     0x19B2
/*#define ALC_QUAD_SOFT                            0x1503*/
#define ALC_SURROUND_5_1_SOFT                    0x1504
#define ALC_SURROUND_6_1_SOFT                    0x1505
#define ALC_SURROUND_7_1_SOFT                    0x1506
#endif

#ifndef AL_SOFT_source_start_delay
#define AL_SOFT_source_start_delay
typedef void (AL_APIENTRY*LPALSOURCEPLAYATTIMESOFT)(ALuint source, ALint64SOFT start_time) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALSOURCEPLAYATTIMEVSOFT)(ALsizei n, const ALuint *sources, ALint64SOFT start_time) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
void AL_APIENTRY alSourcePlayAtTimeSOFT(ALuint source, ALint64SOFT start_time) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePlayAtTimevSOFT(ALsizei n, const ALuint *sources, ALint64SOFT start_time) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_EXT_debug
#define ALC_EXT_debug
#define ALC_CONTEXT_FLAGS_EXT                    0x19CF
#define ALC_CONTEXT_DEBUG_BIT_EXT                0x0001
#endif

#ifndef AL_EXT_debug
#define AL_EXT_debug
#define AL_DONT_CARE_EXT                         0x0002
#define AL_DEBUG_OUTPUT_EXT                      0x19B2
#define AL_DEBUG_CALLBACK_FUNCTION_EXT           0x19B3
#define AL_DEBUG_CALLBACK_USER_PARAM_EXT         0x19B4
#define AL_DEBUG_SOURCE_API_EXT                  0x19B5
#define AL_DEBUG_SOURCE_AUDIO_SYSTEM_EXT         0x19B6
#define AL_DEBUG_SOURCE_THIRD_PARTY_EXT          0x19B7
#define AL_DEBUG_SOURCE_APPLICATION_EXT          0x19B8
#define AL_DEBUG_SOURCE_OTHER_EXT                0x19B9
#define AL_DEBUG_TYPE_ERROR_EXT                  0x19BA
#define AL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_EXT    0x19BB
#define AL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_EXT     0x19BC
#define AL_DEBUG_TYPE_PORTABILITY_EXT            0x19BD
#define AL_DEBUG_TYPE_PERFORMANCE_EXT            0x19BE
#define AL_DEBUG_TYPE_MARKER_EXT                 0x19BF
#define AL_DEBUG_TYPE_PUSH_GROUP_EXT             0x19C0
#define AL_DEBUG_TYPE_POP_GROUP_EXT              0x19C1
#define AL_DEBUG_TYPE_OTHER_EXT                  0x19C2
#define AL_DEBUG_SEVERITY_HIGH_EXT               0x19C3
#define AL_DEBUG_SEVERITY_MEDIUM_EXT             0x19C4
#define AL_DEBUG_SEVERITY_LOW_EXT                0x19C5
#define AL_DEBUG_SEVERITY_NOTIFICATION_EXT       0x19C6
#define AL_DEBUG_LOGGED_MESSAGES_EXT             0x19C7
#define AL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_EXT  0x19C8
#define AL_MAX_DEBUG_MESSAGE_LENGTH_EXT          0x19C9
#define AL_MAX_DEBUG_LOGGED_MESSAGES_EXT         0x19CA
#define AL_MAX_DEBUG_GROUP_STACK_DEPTH_EXT       0x19CB
#define AL_MAX_LABEL_LENGTH_EXT                  0x19CC
#define AL_STACK_OVERFLOW_EXT                    0x19CD
#define AL_STACK_UNDERFLOW_EXT                   0x19CE
#define AL_CONTEXT_FLAGS_EXT                     0x19CF
#define AL_BUFFER_EXT                            0x1009 /* Same as AL_BUFFER */
#define AL_SOURCE_EXT                            0x19D0
#define AL_FILTER_EXT                            0x19D1
#define AL_EFFECT_EXT                            0x19D2
#define AL_AUXILIARY_EFFECT_SLOT_EXT             0x19D3

typedef void (AL_APIENTRY*ALDEBUGPROCEXT)(ALenum source, ALenum type, ALuint id, ALenum severity, ALsizei length, const ALchar *message, void *userParam) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALDEBUGMESSAGECALLBACKEXT)(ALDEBUGPROCEXT callback, void *userParam) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALDEBUGMESSAGEINSERTEXT)(ALenum source, ALenum type, ALuint id, ALenum severity, ALsizei length, const ALchar *message) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALDEBUGMESSAGECONTROLEXT)(ALenum source, ALenum type, ALenum severity, ALsizei count, const ALuint *ids, ALboolean enable) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALPUSHDEBUGGROUPEXT)(ALenum source, ALuint id, ALsizei length, const ALchar *message) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALPOPDEBUGGROUPEXT)(void) AL_API_NOEXCEPT17;
typedef ALuint (AL_APIENTRY*LPALGETDEBUGMESSAGELOGEXT)(ALuint count, ALsizei logBufSize, ALenum *sources, ALenum *types, ALuint *ids, ALenum *severities, ALsizei *lengths, ALchar *logBuf) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALOBJECTLABELEXT)(ALenum identifier, ALuint name, ALsizei length, const ALchar *label) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETOBJECTLABELEXT)(ALenum identifier, ALuint name, ALsizei bufSize, ALsizei *length, ALchar *label) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
void AL_APIENTRY alDebugMessageCallbackEXT(ALDEBUGPROCEXT callback, void *userParam) AL_API_NOEXCEPT;
void AL_APIENTRY alDebugMessageInsertEXT(ALenum source, ALenum type, ALuint id, ALenum severity, ALsizei length, const ALchar *message) AL_API_NOEXCEPT;
void AL_APIENTRY alDebugMessageControlEXT(ALenum source, ALenum type, ALenum severity, ALsizei count, const ALuint *ids, ALboolean enable)  AL_API_NOEXCEPT;
void AL_APIENTRY alPushDebugGroupEXT(ALenum source, ALuint id, ALsizei length, const ALchar *message)  AL_API_NOEXCEPT;
void AL_APIENTRY alPopDebugGroupEXT(void)  AL_API_NOEXCEPT;
ALuint AL_APIENTRY alGetDebugMessageLogEXT(ALuint count, ALsizei logBufSize, ALenum *sources, ALenum *types, ALuint *ids, ALenum *severities, ALsizei *lengths, ALchar *logBuf)  AL_API_NOEXCEPT;
void AL_APIENTRY alObjectLabelEXT(ALenum identifier, ALuint name, ALsizei length, const ALchar *label) AL_API_NOEXCEPT;
void AL_APIENTRY alGetObjectLabelEXT(ALenum identifier, ALuint name, ALsizei bufSize, ALsizei *length, ALchar *label) AL_API_NOEXCEPT;
#endif
#endif

#ifndef ALC_SOFT_system_events
#define ALC_SOFT_system_events
#define ALC_PLAYBACK_DEVICE_SOFT                 0x19D4
#define ALC_CAPTURE_DEVICE_SOFT                  0x19D5
#define ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT 0x19D6
#define ALC_EVENT_TYPE_DEVICE_ADDED_SOFT         0x19D7
#define ALC_EVENT_TYPE_DEVICE_REMOVED_SOFT       0x19D8
#define ALC_EVENT_SUPPORTED_SOFT                 0x19D9
#define ALC_EVENT_NOT_SUPPORTED_SOFT             0x19DA
typedef void (ALC_APIENTRY*ALCEVENTPROCTYPESOFT)(ALCenum eventType, ALCenum deviceType,
    ALCdevice *device, ALCsizei length, const ALCchar *message, void *userParam) ALC_API_NOEXCEPT17;
typedef ALCenum (ALC_APIENTRY*LPALCEVENTISSUPPORTEDSOFT)(ALCenum eventType, ALCenum deviceType) ALC_API_NOEXCEPT17;
typedef ALCboolean (ALC_APIENTRY*LPALCEVENTCONTROLSOFT)(ALCsizei count, const ALCenum *events, ALCboolean enable) ALC_API_NOEXCEPT17;
typedef void (ALC_APIENTRY*LPALCEVENTCALLBACKSOFT)(ALCEVENTPROCTYPESOFT callback, void *userParam) ALC_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALCenum ALC_APIENTRY alcEventIsSupportedSOFT(ALCenum eventType, ALCenum deviceType) ALC_API_NOEXCEPT;
ALCboolean ALC_APIENTRY alcEventControlSOFT(ALCsizei count, const ALCenum *events, ALCboolean enable) ALC_API_NOEXCEPT;
void ALC_APIENTRY alcEventCallbackSOFT(ALCEVENTPROCTYPESOFT callback, void *userParam) ALC_API_NOEXCEPT;
#endif
#endif

#ifndef AL_EXT_direct_context
#define AL_EXT_direct_context
typedef ALCvoid* (ALC_APIENTRY *LPALCGETPROCADDRESS2)(ALCdevice *device, const ALCchar *funcname) AL_API_NOEXCEPT17;

typedef void (AL_APIENTRY *LPALENABLEDIRECT)(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDISABLEDIRECT)(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISENABLEDDIRECT)(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDOPPLERFACTORDIRECT)(ALCcontext *context, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSPEEDOFSOUNDDIRECT)(ALCcontext *context, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDISTANCEMODELDIRECT)(ALCcontext *context, ALenum distanceModel) AL_API_NOEXCEPT17;
typedef const ALchar* (AL_APIENTRY *LPALGETSTRINGDIRECT)(ALCcontext *context, ALenum param) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBOOLEANVDIRECT)(ALCcontext *context, ALenum param, ALboolean *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETINTEGERVDIRECT)(ALCcontext *context, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETFLOATVDIRECT)(ALCcontext *context, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETDOUBLEVDIRECT)(ALCcontext *context, ALenum param, ALdouble *values) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALGETBOOLEANDIRECT)(ALCcontext *context, ALenum param) AL_API_NOEXCEPT17;
typedef ALint (AL_APIENTRY *LPALGETINTEGERDIRECT)(ALCcontext *context, ALenum param) AL_API_NOEXCEPT17;
typedef ALfloat (AL_APIENTRY *LPALGETFLOATDIRECT)(ALCcontext *context, ALenum param) AL_API_NOEXCEPT17;
typedef ALdouble (AL_APIENTRY *LPALGETDOUBLEDIRECT)(ALCcontext *context, ALenum param) AL_API_NOEXCEPT17;
typedef ALenum (AL_APIENTRY *LPALGETERRORDIRECT)(ALCcontext *context) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISEXTENSIONPRESENTDIRECT)(ALCcontext *context, const ALchar *extname) AL_API_NOEXCEPT17;
typedef void* (AL_APIENTRY *LPALGETPROCADDRESSDIRECT)(ALCcontext *context, const ALchar *fname) AL_API_NOEXCEPT17;
typedef ALenum (AL_APIENTRY *LPALGETENUMVALUEDIRECT)(ALCcontext *context, const ALchar *ename) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENERFDIRECT)(ALCcontext *context, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENER3FDIRECT)(ALCcontext *context, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENERFVDIRECT)(ALCcontext *context, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENERIDIRECT)(ALCcontext *context, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENER3IDIRECT)(ALCcontext *context, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALLISTENERIVDIRECT)(ALCcontext *context, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENERFDIRECT)(ALCcontext *context, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENER3FDIRECT)(ALCcontext *context, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENERFVDIRECT)(ALCcontext *context, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENERIDIRECT)(ALCcontext *context, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENER3IDIRECT)(ALCcontext *context, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETLISTENERIVDIRECT)(ALCcontext *context, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGENSOURCESDIRECT)(ALCcontext *context, ALsizei n, ALuint *sources) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDELETESOURCESDIRECT)(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISSOURCEDIRECT)(ALCcontext *context, ALuint source) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEFDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCE3FDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEFVDIRECT)(ALCcontext *context, ALuint source, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEIDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCE3IDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEIVDIRECT)(ALCcontext *context, ALuint source, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEFDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCE3FDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEFVDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEIDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCE3IDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEIVDIRECT)(ALCcontext *context, ALuint source, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEPLAYDIRECT)(ALCcontext *context, ALuint source) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCESTOPDIRECT)(ALCcontext *context, ALuint source) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEREWINDDIRECT)(ALCcontext *context, ALuint source) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEPAUSEDIRECT)(ALCcontext *context, ALuint source) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEPLAYVDIRECT)(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCESTOPVDIRECT)(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEREWINDVDIRECT)(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEPAUSEVDIRECT)(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEQUEUEBUFFERSDIRECT)(ALCcontext *context, ALuint source, ALsizei nb, const ALuint *buffers) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEUNQUEUEBUFFERSDIRECT)(ALCcontext *context, ALuint source, ALsizei nb, ALuint *buffers) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGENBUFFERSDIRECT)(ALCcontext *context, ALsizei n, ALuint *buffers) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDELETEBUFFERSDIRECT)(ALCcontext *context, ALsizei n, const ALuint *buffers) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISBUFFERDIRECT)(ALCcontext *context, ALuint buffer) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFERDATADIRECT)(ALCcontext *context, ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei samplerate) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFERFDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFER3FDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFERFVDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFERIDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFER3IDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALBUFFERIVDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERFDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFER3FDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERFVDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERIDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFER3IDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERIVDIRECT)(ALCcontext *context, ALuint buffer, ALenum param, ALint *values) AL_API_NOEXCEPT17;
/* ALC_EXT_EFX */
typedef void (AL_APIENTRY *LPALGENEFFECTSDIRECT)(ALCcontext *context, ALsizei n, ALuint *effects) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDELETEEFFECTSDIRECT)(ALCcontext *context, ALsizei n, const ALuint *effects) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISEFFECTDIRECT)(ALCcontext *context, ALuint effect) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALEFFECTIDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALEFFECTIVDIRECT)(ALCcontext *context, ALuint effect, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALEFFECTFDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALEFFECTFVDIRECT)(ALCcontext *context, ALuint effect, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETEFFECTIDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETEFFECTIVDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETEFFECTFDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETEFFECTFVDIRECT)(ALCcontext *context, ALuint effect, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGENFILTERSDIRECT)(ALCcontext *context, ALsizei n, ALuint *filters) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDELETEFILTERSDIRECT)(ALCcontext *context, ALsizei n, const ALuint *filters) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISFILTERDIRECT)(ALCcontext *context, ALuint filter) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALFILTERIDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALFILTERIVDIRECT)(ALCcontext *context, ALuint filter, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALFILTERFDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALFILTERFVDIRECT)(ALCcontext *context, ALuint filter, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETFILTERIDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETFILTERIVDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETFILTERFDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETFILTERFVDIRECT)(ALCcontext *context, ALuint filter, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGENAUXILIARYEFFECTSLOTSDIRECT)(ALCcontext *context, ALsizei n, ALuint *effectslots) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALDELETEAUXILIARYEFFECTSLOTSDIRECT)(ALCcontext *context, ALsizei n, const ALuint *effectslots) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPALISAUXILIARYEFFECTSLOTDIRECT)(ALCcontext *context, ALuint effectslot) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTIDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALint value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTIVDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, const ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTFDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTFVDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, const ALfloat *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTIDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALint *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTIVDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALint *values) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTFDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat *value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTFVDIRECT)(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat *values) AL_API_NOEXCEPT17;
/* AL_EXT_BUFFER_DATA_STATIC */
typedef void (AL_APIENTRY *LPALBUFFERDATASTATICDIRECT)(ALCcontext *context, ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq) AL_API_NOEXCEPT17;
/* AL_EXT_debug */
typedef void (AL_APIENTRY*LPALDEBUGMESSAGECALLBACKDIRECTEXT)(ALCcontext *context, ALDEBUGPROCEXT callback, void *userParam) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALDEBUGMESSAGEINSERTDIRECTEXT)(ALCcontext *context, ALenum source, ALenum type, ALuint id, ALenum severity, ALsizei length, const ALchar *message) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALDEBUGMESSAGECONTROLDIRECTEXT)(ALCcontext *context, ALenum source, ALenum type, ALenum severity, ALsizei count, const ALuint *ids, ALboolean enable) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALPUSHDEBUGGROUPDIRECTEXT)(ALCcontext *context, ALenum source, ALuint id, ALsizei length, const ALchar *message) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALPOPDEBUGGROUPDIRECTEXT)(ALCcontext *context) AL_API_NOEXCEPT17;
typedef ALuint (AL_APIENTRY*LPALGETDEBUGMESSAGELOGDIRECTEXT)(ALCcontext *context, ALuint count, ALsizei logBufSize, ALenum *sources, ALenum *types, ALuint *ids, ALenum *severities, ALsizei *lengths, ALchar *logBuf) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALOBJECTLABELDIRECTEXT)(ALCcontext *context, ALenum identifier, ALuint name, ALsizei length, const ALchar *label) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY*LPALGETOBJECTLABELDIRECTEXT)(ALCcontext *context, ALenum identifier, ALuint name, ALsizei bufSize, ALsizei *length, ALchar *label) AL_API_NOEXCEPT17;
/* AL_EXT_FOLDBACK */
typedef void (AL_APIENTRY *LPALREQUESTFOLDBACKSTARTDIRECT)(ALCcontext *context, ALenum mode, ALsizei count, ALsizei length, ALfloat *mem, LPALFOLDBACKCALLBACK callback) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALREQUESTFOLDBACKSTOPDIRECT)(ALCcontext *context) AL_API_NOEXCEPT17;
/* AL_SOFT_buffer_sub_data */
typedef void (AL_APIENTRY *LPALBUFFERSUBDATADIRECTSOFT)(ALCcontext *context, ALuint buffer, ALenum format, const ALvoid *data, ALsizei offset, ALsizei length) AL_API_NOEXCEPT17;
/* AL_SOFT_source_latency */
typedef void (AL_APIENTRY *LPALSOURCEDDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALdouble) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCE3DDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALdouble,ALdouble,ALdouble) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEDVDIRECTSOFT)(ALCcontext*,ALuint,ALenum,const ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEDDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCE3DDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALdouble*,ALdouble*,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEDVDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALdouble*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEI64DIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALint64SOFT) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCE3I64DIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALint64SOFT,ALint64SOFT,ALint64SOFT) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEI64VDIRECTSOFT)(ALCcontext*,ALuint,ALenum,const ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEI64DIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCE3I64DIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALint64SOFT*,ALint64SOFT*,ALint64SOFT*) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETSOURCEI64VDIRECTSOFT)(ALCcontext*,ALuint,ALenum,ALint64SOFT*) AL_API_NOEXCEPT17;
/* AL_SOFT_deferred_updates */
typedef void (AL_APIENTRY *LPALDEFERUPDATESDIRECTSOFT)(ALCcontext *context) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALPROCESSUPDATESDIRECTSOFT)(ALCcontext *context) AL_API_NOEXCEPT17;
/* AL_SOFT_source_resampler */
typedef const ALchar* (AL_APIENTRY *LPALGETSTRINGIDIRECTSOFT)(ALCcontext *context, ALenum pname, ALsizei index) AL_API_NOEXCEPT17;
/* AL_SOFT_events */
typedef void (AL_APIENTRY *LPALEVENTCONTROLDIRECTSOFT)(ALCcontext *context, ALsizei count, const ALenum *types, ALboolean enable) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALEVENTCALLBACKDIRECTSOFT)(ALCcontext *context, ALEVENTPROCSOFT callback, void *userParam) AL_API_NOEXCEPT17;
typedef void* (AL_APIENTRY *LPALGETPOINTERDIRECTSOFT)(ALCcontext *context, ALenum pname) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETPOINTERVDIRECTSOFT)(ALCcontext *context, ALenum pname, void **values) AL_API_NOEXCEPT17;
/* AL_SOFT_callback_buffer */
typedef void (AL_APIENTRY *LPALBUFFERCALLBACKDIRECTSOFT)(ALCcontext *context, ALuint buffer, ALenum format, ALsizei freq, ALBUFFERCALLBACKTYPESOFT callback, ALvoid *userptr) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERPTRDIRECTSOFT)(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **value) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFER3PTRDIRECTSOFT)(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **value1, ALvoid **value2, ALvoid **value3) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALGETBUFFERPTRVDIRECTSOFT)(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **values) AL_API_NOEXCEPT17;
/* AL_SOFT_source_start_delay */
typedef void (AL_APIENTRY *LPALSOURCEPLAYATTIMEDIRECTSOFT)(ALCcontext *context, ALuint source, ALint64SOFT start_time) AL_API_NOEXCEPT17;
typedef void (AL_APIENTRY *LPALSOURCEPLAYATTIMEVDIRECTSOFT)(ALCcontext *context, ALsizei n, const ALuint *sources, ALint64SOFT start_time) AL_API_NOEXCEPT17;
/* EAX */
typedef ALenum (AL_APIENTRY *LPEAXSETDIRECT)(ALCcontext *context, const struct _GUID *property_set_id, ALuint property_id, ALuint source_id, ALvoid *value, ALuint value_size) AL_API_NOEXCEPT17;
typedef ALenum (AL_APIENTRY *LPEAXGETDIRECT)(ALCcontext *context, const struct _GUID *property_set_id, ALuint property_id, ALuint source_id, ALvoid *value, ALuint value_size) AL_API_NOEXCEPT17;
typedef ALboolean (AL_APIENTRY *LPEAXSETBUFFERMODEDIRECT)(ALCcontext *context, ALsizei n, const ALuint *buffers, ALint value) AL_API_NOEXCEPT17;
typedef ALenum (AL_APIENTRY *LPEAXGETBUFFERMODEDIRECT)(ALCcontext *context, ALuint buffer, ALint *pReserved) AL_API_NOEXCEPT17;
#ifdef AL_ALEXT_PROTOTYPES
ALCvoid* AL_APIENTRY alcGetProcAddress2(ALCdevice *device, const ALchar *funcName) AL_API_NOEXCEPT;

void AL_APIENTRY alEnableDirect(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT;
void AL_APIENTRY alDisableDirect(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsEnabledDirect(ALCcontext *context, ALenum capability) AL_API_NOEXCEPT;

void AL_APIENTRY alDopplerFactorDirect(ALCcontext *context, ALfloat value) AL_API_NOEXCEPT;
void AL_APIENTRY alSpeedOfSoundDirect(ALCcontext *context, ALfloat value) AL_API_NOEXCEPT;
void AL_APIENTRY alDistanceModelDirect(ALCcontext *context, ALenum distanceModel) AL_API_NOEXCEPT;

const ALchar* AL_APIENTRY alGetStringDirect(ALCcontext *context, ALenum param) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBooleanvDirect(ALCcontext *context, ALenum param, ALboolean *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetIntegervDirect(ALCcontext *context, ALenum param, ALint *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetFloatvDirect(ALCcontext *context, ALenum param, ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetDoublevDirect(ALCcontext *context, ALenum param, ALdouble *values) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alGetBooleanDirect(ALCcontext *context, ALenum param) AL_API_NOEXCEPT;
ALint AL_APIENTRY alGetIntegerDirect(ALCcontext *context, ALenum param) AL_API_NOEXCEPT;
ALfloat AL_APIENTRY alGetFloatDirect(ALCcontext *context, ALenum param) AL_API_NOEXCEPT;
ALdouble AL_APIENTRY alGetDoubleDirect(ALCcontext *context, ALenum param) AL_API_NOEXCEPT;

ALenum AL_APIENTRY alGetErrorDirect(ALCcontext *context) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsExtensionPresentDirect(ALCcontext *context, const ALchar *extname) AL_API_NOEXCEPT;
void* AL_APIENTRY alGetProcAddressDirect(ALCcontext *context, const ALchar *fname) AL_API_NOEXCEPT;
ALenum AL_APIENTRY alGetEnumValueDirect(ALCcontext *context, const ALchar *ename) AL_API_NOEXCEPT;

void AL_APIENTRY alListenerfDirect(ALCcontext *context, ALenum param, ALfloat value) AL_API_NOEXCEPT;
void AL_APIENTRY alListener3fDirect(ALCcontext *context, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT;
void AL_APIENTRY alListenerfvDirect(ALCcontext *context, ALenum param, const ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alListeneriDirect(ALCcontext *context, ALenum param, ALint value) AL_API_NOEXCEPT;
void AL_APIENTRY alListener3iDirect(ALCcontext *context, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT;
void AL_APIENTRY alListenerivDirect(ALCcontext *context, ALenum param, const ALint *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListenerfDirect(ALCcontext *context, ALenum param, ALfloat *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListener3fDirect(ALCcontext *context, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListenerfvDirect(ALCcontext *context, ALenum param, ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListeneriDirect(ALCcontext *context, ALenum param, ALint *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListener3iDirect(ALCcontext *context, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetListenerivDirect(ALCcontext *context, ALenum param, ALint *values) AL_API_NOEXCEPT;

void AL_APIENTRY alGenSourcesDirect(ALCcontext *context, ALsizei n, ALuint *sources) AL_API_NOEXCEPT;
void AL_APIENTRY alDeleteSourcesDirect(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsSourceDirect(ALCcontext *context, ALuint source) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcefDirect(ALCcontext *context, ALuint source, ALenum param, ALfloat value) AL_API_NOEXCEPT;
void AL_APIENTRY alSource3fDirect(ALCcontext *context, ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcefvDirect(ALCcontext *context, ALuint source, ALenum param, const ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceiDirect(ALCcontext *context, ALuint source, ALenum param, ALint value) AL_API_NOEXCEPT;
void AL_APIENTRY alSource3iDirect(ALCcontext *context, ALuint source, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceivDirect(ALCcontext *context, ALuint source, ALenum param, const ALint *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcefDirect(ALCcontext *context, ALuint source, ALenum param, ALfloat *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSource3fDirect(ALCcontext *context, ALuint source, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcefvDirect(ALCcontext *context, ALuint source, ALenum param, ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourceiDirect(ALCcontext *context, ALuint source,  ALenum param, ALint *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSource3iDirect(ALCcontext *context, ALuint source, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourceivDirect(ALCcontext *context, ALuint source,  ALenum param, ALint *values) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePlayDirect(ALCcontext *context, ALuint source) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceStopDirect(ALCcontext *context, ALuint source) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceRewindDirect(ALCcontext *context, ALuint source) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePauseDirect(ALCcontext *context, ALuint source) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePlayvDirect(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceStopvDirect(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceRewindvDirect(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePausevDirect(ALCcontext *context, ALsizei n, const ALuint *sources) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceQueueBuffersDirect(ALCcontext *context, ALuint source, ALsizei nb, const ALuint *buffers) AL_API_NOEXCEPT;
void AL_APIENTRY alSourceUnqueueBuffersDirect(ALCcontext *context, ALuint source, ALsizei nb, ALuint *buffers) AL_API_NOEXCEPT;

void AL_APIENTRY alGenBuffersDirect(ALCcontext *context, ALsizei n, ALuint *buffers) AL_API_NOEXCEPT;
void AL_APIENTRY alDeleteBuffersDirect(ALCcontext *context, ALsizei n, const ALuint *buffers) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsBufferDirect(ALCcontext *context, ALuint buffer) AL_API_NOEXCEPT;
void AL_APIENTRY alBufferDataDirect(ALCcontext *context, ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei samplerate) AL_API_NOEXCEPT;
void AL_APIENTRY alBufferfDirect(ALCcontext *context, ALuint buffer, ALenum param, ALfloat value) AL_API_NOEXCEPT;
void AL_APIENTRY alBuffer3fDirect(ALCcontext *context, ALuint buffer, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) AL_API_NOEXCEPT;
void AL_APIENTRY alBufferfvDirect(ALCcontext *context, ALuint buffer, ALenum param, const ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alBufferiDirect(ALCcontext *context, ALuint buffer, ALenum param, ALint value) AL_API_NOEXCEPT;
void AL_APIENTRY alBuffer3iDirect(ALCcontext *context, ALuint buffer, ALenum param, ALint value1, ALint value2, ALint value3) AL_API_NOEXCEPT;
void AL_APIENTRY alBufferivDirect(ALCcontext *context, ALuint buffer, ALenum param, const ALint *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferfDirect(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBuffer3fDirect(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferfvDirect(ALCcontext *context, ALuint buffer, ALenum param, ALfloat *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferiDirect(ALCcontext *context, ALuint buffer, ALenum param, ALint *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBuffer3iDirect(ALCcontext *context, ALuint buffer, ALenum param, ALint *value1, ALint *value2, ALint *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferivDirect(ALCcontext *context, ALuint buffer, ALenum param, ALint *values) AL_API_NOEXCEPT;

void AL_APIENTRY alGenEffectsDirect(ALCcontext *context, ALsizei n, ALuint *effects) AL_API_NOEXCEPT;
void AL_APIENTRY alDeleteEffectsDirect(ALCcontext *context, ALsizei n, const ALuint *effects) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsEffectDirect(ALCcontext *context, ALuint effect) AL_API_NOEXCEPT;
void AL_APIENTRY alEffectiDirect(ALCcontext *context, ALuint effect, ALenum param, ALint iValue) AL_API_NOEXCEPT;
void AL_APIENTRY alEffectivDirect(ALCcontext *context, ALuint effect, ALenum param, const ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alEffectfDirect(ALCcontext *context, ALuint effect, ALenum param, ALfloat flValue) AL_API_NOEXCEPT;
void AL_APIENTRY alEffectfvDirect(ALCcontext *context, ALuint effect, ALenum param, const ALfloat *pflValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetEffectiDirect(ALCcontext *context, ALuint effect, ALenum param, ALint *piValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetEffectivDirect(ALCcontext *context, ALuint effect, ALenum param, ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetEffectfDirect(ALCcontext *context, ALuint effect, ALenum param, ALfloat *pflValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetEffectfvDirect(ALCcontext *context, ALuint effect, ALenum param, ALfloat *pflValues) AL_API_NOEXCEPT;

void AL_APIENTRY alGenFiltersDirect(ALCcontext *context, ALsizei n, ALuint *filters) AL_API_NOEXCEPT;
void AL_APIENTRY alDeleteFiltersDirect(ALCcontext *context, ALsizei n, const ALuint *filters) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsFilterDirect(ALCcontext *context, ALuint filter) AL_API_NOEXCEPT;
void AL_APIENTRY alFilteriDirect(ALCcontext *context, ALuint filter, ALenum param, ALint iValue) AL_API_NOEXCEPT;
void AL_APIENTRY alFilterivDirect(ALCcontext *context, ALuint filter, ALenum param, const ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alFilterfDirect(ALCcontext *context, ALuint filter, ALenum param, ALfloat flValue) AL_API_NOEXCEPT;
void AL_APIENTRY alFilterfvDirect(ALCcontext *context, ALuint filter, ALenum param, const ALfloat *pflValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetFilteriDirect(ALCcontext *context, ALuint filter, ALenum param, ALint *piValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetFilterivDirect(ALCcontext *context, ALuint filter, ALenum param, ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetFilterfDirect(ALCcontext *context, ALuint filter, ALenum param, ALfloat *pflValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetFilterfvDirect(ALCcontext *context, ALuint filter, ALenum param, ALfloat *pflValues) AL_API_NOEXCEPT;

void AL_APIENTRY alGenAuxiliaryEffectSlotsDirect(ALCcontext *context, ALsizei n, ALuint *effectslots) AL_API_NOEXCEPT;
void AL_APIENTRY alDeleteAuxiliaryEffectSlotsDirect(ALCcontext *context, ALsizei n, const ALuint *effectslots) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY alIsAuxiliaryEffectSlotDirect(ALCcontext *context, ALuint effectslot) AL_API_NOEXCEPT;
void AL_APIENTRY alAuxiliaryEffectSlotiDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALint iValue) AL_API_NOEXCEPT;
void AL_APIENTRY alAuxiliaryEffectSlotivDirect(ALCcontext *context, ALuint effectslot, ALenum param, const ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alAuxiliaryEffectSlotfDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat flValue) AL_API_NOEXCEPT;
void AL_APIENTRY alAuxiliaryEffectSlotfvDirect(ALCcontext *context, ALuint effectslot, ALenum param, const ALfloat *pflValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetAuxiliaryEffectSlotiDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALint *piValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetAuxiliaryEffectSlotivDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALint *piValues) AL_API_NOEXCEPT;
void AL_APIENTRY alGetAuxiliaryEffectSlotfDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat *pflValue) AL_API_NOEXCEPT;
void AL_APIENTRY alGetAuxiliaryEffectSlotfvDirect(ALCcontext *context, ALuint effectslot, ALenum param, ALfloat *pflValues) AL_API_NOEXCEPT;

void AL_APIENTRY alBufferDataStaticDirect(ALCcontext *context, ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq) AL_API_NOEXCEPT;

void AL_APIENTRY alDebugMessageCallbackDirectEXT(ALCcontext *context, ALDEBUGPROCEXT callback, void *userParam)  AL_API_NOEXCEPT;
void AL_APIENTRY alDebugMessageInsertDirectEXT(ALCcontext *context, ALenum source, ALenum type, ALuint id, ALenum severity, ALsizei length, const ALchar *message)  AL_API_NOEXCEPT;
void AL_APIENTRY alDebugMessageControlDirectEXT(ALCcontext *context, ALenum source, ALenum type, ALenum severity, ALsizei count, const ALuint *ids, ALboolean enable) AL_API_NOEXCEPT;
void AL_APIENTRY alPushDebugGroupDirectEXT(ALCcontext *context, ALenum source, ALuint id, ALsizei length, const ALchar *message) AL_API_NOEXCEPT;
void AL_APIENTRY alPopDebugGroupDirectEXT(ALCcontext *context) AL_API_NOEXCEPT;
ALuint AL_APIENTRY alGetDebugMessageLogDirectEXT(ALCcontext *context, ALuint count, ALsizei logBufSize, ALenum *sources, ALenum *types, ALuint *ids, ALenum *severities, ALsizei *lengths, ALchar *logBuf) AL_API_NOEXCEPT;
void AL_APIENTRY alObjectLabelDirectEXT(ALCcontext *context, ALenum identifier, ALuint name, ALsizei length, const ALchar *label) AL_API_NOEXCEPT;
void AL_APIENTRY alGetObjectLabelDirectEXT(ALCcontext *context, ALenum identifier, ALuint name, ALsizei bufSize, ALsizei *length, ALchar *label) AL_API_NOEXCEPT;

void AL_APIENTRY alRequestFoldbackStartDirect(ALCcontext *context, ALenum mode, ALsizei count, ALsizei length, ALfloat *mem, LPALFOLDBACKCALLBACK callback) AL_API_NOEXCEPT;
void AL_APIENTRY alRequestFoldbackStopDirect(ALCcontext *context) AL_API_NOEXCEPT;

void AL_APIENTRY alBufferSubDataDirectSOFT(ALCcontext *context, ALuint buffer, ALenum format, const ALvoid *data, ALsizei offset, ALsizei length) AL_API_NOEXCEPT;

void AL_APIENTRY alSourcedDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALdouble value) AL_API_NOEXCEPT;
void AL_APIENTRY alSource3dDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALdouble value1, ALdouble value2, ALdouble value3) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcedvDirectSOFT(ALCcontext *context, ALuint source, ALenum param, const ALdouble *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcedDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALdouble *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSource3dDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALdouble *value1, ALdouble *value2, ALdouble *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcedvDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALdouble *values) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcei64DirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALint64SOFT value) AL_API_NOEXCEPT;
void AL_APIENTRY alSource3i64DirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALint64SOFT value1, ALint64SOFT value2, ALint64SOFT value3) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcei64vDirectSOFT(ALCcontext *context, ALuint source, ALenum param, const ALint64SOFT *values) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcei64DirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALint64SOFT *value) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSource3i64DirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALint64SOFT *value1, ALint64SOFT *value2, ALint64SOFT *value3) AL_API_NOEXCEPT;
void AL_APIENTRY alGetSourcei64vDirectSOFT(ALCcontext *context, ALuint source, ALenum param, ALint64SOFT *values) AL_API_NOEXCEPT;

void AL_APIENTRY alDeferUpdatesDirectSOFT(ALCcontext *context) AL_API_NOEXCEPT;
void AL_APIENTRY alProcessUpdatesDirectSOFT(ALCcontext *context) AL_API_NOEXCEPT;

const ALchar* AL_APIENTRY alGetStringiDirectSOFT(ALCcontext *context, ALenum pname, ALsizei index) AL_API_NOEXCEPT;

void AL_APIENTRY alEventControlDirectSOFT(ALCcontext *context, ALsizei count, const ALenum *types, ALboolean enable) AL_API_NOEXCEPT;
void AL_APIENTRY alEventCallbackDirectSOFT(ALCcontext *context, ALEVENTPROCSOFT callback, void *userParam) AL_API_NOEXCEPT;
void* AL_APIENTRY alGetPointerDirectSOFT(ALCcontext *context, ALenum pname) AL_API_NOEXCEPT;
void AL_APIENTRY alGetPointervDirectSOFT(ALCcontext *context, ALenum pname, void **values) AL_API_NOEXCEPT;

void AL_APIENTRY alBufferCallbackDirectSOFT(ALCcontext *context, ALuint buffer, ALenum format, ALsizei freq, ALBUFFERCALLBACKTYPESOFT callback, ALvoid *userptr) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferPtrDirectSOFT(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **ptr) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBuffer3PtrDirectSOFT(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **ptr0, ALvoid **ptr1, ALvoid **ptr2) AL_API_NOEXCEPT;
void AL_APIENTRY alGetBufferPtrvDirectSOFT(ALCcontext *context, ALuint buffer, ALenum param, ALvoid **ptr) AL_API_NOEXCEPT;

void AL_APIENTRY alSourcePlayAtTimeDirectSOFT(ALCcontext *context, ALuint source, ALint64SOFT start_time) AL_API_NOEXCEPT;
void AL_APIENTRY alSourcePlayAtTimevDirectSOFT(ALCcontext *context, ALsizei n, const ALuint *sources, ALint64SOFT start_time) AL_API_NOEXCEPT;

ALenum AL_APIENTRY EAXSetDirect(ALCcontext *context, const struct _GUID *property_set_id, ALuint property_id, ALuint source_id, ALvoid *value, ALuint value_size) AL_API_NOEXCEPT;
ALenum AL_APIENTRY EAXGetDirect(ALCcontext *context, const struct _GUID *property_set_id, ALuint property_id, ALuint source_id, ALvoid *value, ALuint value_size) AL_API_NOEXCEPT;
ALboolean AL_APIENTRY EAXSetBufferModeDirect(ALCcontext *context, ALsizei n, const ALuint *buffers, ALint value) AL_API_NOEXCEPT;
ALenum AL_APIENTRY EAXGetBufferModeDirect(ALCcontext *context, ALuint buffer, ALint *pReserved) AL_API_NOEXCEPT;
#endif
#endif

#ifdef __cplusplus
}
#endif
/* NOLINTEND */

#endif
