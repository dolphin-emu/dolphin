/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/*
  The PulseAudio target for SDL 1.3 is based on the 1.3 arts target, with
   the appropriate parts replaced with the 1.2 PulseAudio target code. This
   was the cleanest way to move it to 1.3. The 1.2 target was written by
   Stéphan Kochen: stephan .a.t. kochen.nl
*/

#include "SDL_config.h"

/* Allow access to a raw mixing buffer */

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <pulse/simple.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "SDL_pulseaudio.h"

#ifdef SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC
#include "SDL_name.h"
#include "SDL_loadso.h"
#else
#define SDL_NAME(X)	X
#endif

/* The tag name used by pulse audio */
#define PULSEAUDIO_DRIVER_NAME         "pulseaudio"

#ifdef SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC

static const char *pulse_library = SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC;
static void *pulse_handle = NULL;

/* !!! FIXME: I hate this SDL_NAME clutter...it makes everything so messy! */
static pa_simple *(*SDL_NAME(pa_simple_new)) (const char *server,
                                              const char *name,
                                              pa_stream_direction_t dir,
                                              const char *dev,
                                              const char *stream_name,
                                              const pa_sample_spec * ss,
                                              const pa_channel_map * map,
                                              const pa_buffer_attr * attr,
                                              int *error);
static void (*SDL_NAME(pa_simple_free)) (pa_simple * s);
static int (*SDL_NAME(pa_simple_drain)) (pa_simple * s, int *error);
static int (*SDL_NAME(pa_simple_write)) (pa_simple * s,
                                         const void *data,
                                         size_t length, int *error);
static pa_channel_map *(*SDL_NAME(pa_channel_map_init_auto)) (pa_channel_map *
                                                              m,
                                                              unsigned
                                                              channels,
                                                              pa_channel_map_def_t
                                                              def);
static const char *(*SDL_NAME(pa_strerror)) (int error);


#define SDL_PULSEAUDIO_SYM(x) { #x, (void **) (char *) &SDL_NAME(x) }
static struct
{
    const char *name;
    void **func;
} pulse_functions[] = {
/* *INDENT-OFF* */
    SDL_PULSEAUDIO_SYM(pa_simple_new),
    SDL_PULSEAUDIO_SYM(pa_simple_free),
    SDL_PULSEAUDIO_SYM(pa_simple_drain),
    SDL_PULSEAUDIO_SYM(pa_simple_write),
    SDL_PULSEAUDIO_SYM(pa_channel_map_init_auto),
    SDL_PULSEAUDIO_SYM(pa_strerror),
/* *INDENT-ON* */
};

#undef SDL_PULSEAUDIO_SYM

static void
UnloadPulseLibrary()
{
    if (pulse_handle != NULL) {
        SDL_UnloadObject(pulse_handle);
        pulse_handle = NULL;
    }
}

static int
LoadPulseLibrary(void)
{
    int i, retval = -1;

    if (pulse_handle == NULL) {
        pulse_handle = SDL_LoadObject(pulse_library);
        if (pulse_handle != NULL) {
            retval = 0;
            for (i = 0; i < SDL_arraysize(pulse_functions); ++i) {
                *pulse_functions[i].func =
                    SDL_LoadFunction(pulse_handle, pulse_functions[i].name);
                if (!*pulse_functions[i].func) {
                    retval = -1;
                    UnloadPulseLibrary();
                    break;
                }
            }
        }
    }

    return retval;
}

#else

static void
UnloadPulseLibrary()
{
    return;
}

static int
LoadPulseLibrary(void)
{
    return 0;
}

#endif /* SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC */

/* This function waits until it is possible to write a full sound buffer */
static void
PULSEAUDIO_WaitDevice(_THIS)
{
    Sint32 ticks;

    /* Check to see if the thread-parent process is still alive */
    {
        static int cnt = 0;
        /* Note that this only works with thread implementations
           that use a different process id for each thread.
         */
        /* Check every 10 loops */
        if (this->hidden->parent && (((++cnt) % 10) == 0)) {
            if (kill(this->hidden->parent, 0) < 0 && errno == ESRCH) {
                this->enabled = 0;
            }
        }
    }

    /* Use timer for general audio synchronization */
    ticks =
        ((Sint32) (this->hidden->next_frame - SDL_GetTicks())) - FUDGE_TICKS;
    if (ticks > 0) {
        SDL_Delay(ticks);
    }
}

static void
PULSEAUDIO_PlayDevice(_THIS)
{
    /* Write the audio data */
    if (SDL_NAME(pa_simple_write) (this->hidden->stream, this->hidden->mixbuf,
                                   this->hidden->mixlen, NULL) != 0) {
        this->enabled = 0;
    }
}

static void
PULSEAUDIO_WaitDone(_THIS)
{
    SDL_NAME(pa_simple_drain) (this->hidden->stream, NULL);
}


static Uint8 *
PULSEAUDIO_GetDeviceBuf(_THIS)
{
    return (this->hidden->mixbuf);
}


static void
PULSEAUDIO_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        if (this->hidden->mixbuf != NULL) {
            SDL_FreeAudioMem(this->hidden->mixbuf);
            this->hidden->mixbuf = NULL;
        }
        if (this->hidden->stream) {
            SDL_NAME(pa_simple_drain) (this->hidden->stream, NULL);
            SDL_NAME(pa_simple_free) (this->hidden->stream);
            this->hidden->stream = NULL;
        }
        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}


/* !!! FIXME: this could probably be expanded. */
/* Try to get the name of the program */
static char *
get_progname(void)
{
    char *progname = NULL;
#ifdef __LINUX__
    FILE *fp;
    static char temp[BUFSIZ];

    SDL_snprintf(temp, SDL_arraysize(temp), "/proc/%d/cmdline", getpid());
    fp = fopen(temp, "r");
    if (fp != NULL) {
        if (fgets(temp, sizeof(temp) - 1, fp)) {
            progname = SDL_strrchr(temp, '/');
            if (progname == NULL) {
                progname = temp;
            } else {
                progname = progname + 1;
            }
        }
        fclose(fp);
    }
#endif
    return (progname);
}


static int
PULSEAUDIO_OpenDevice(_THIS, const char *devname, int iscapture)
{
    Uint16 test_format = 0;
    pa_sample_spec paspec;
    pa_buffer_attr paattr;
    pa_channel_map pacmap;
    int err = 0;

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        SDL_OutOfMemory();
        return 0;
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));

    paspec.format = PA_SAMPLE_INVALID;

    /* Try for a closest match on audio format */
    for (test_format = SDL_FirstAudioFormat(this->spec.format);
         (paspec.format == PA_SAMPLE_INVALID) && test_format;) {
#ifdef DEBUG_AUDIO
        fprintf(stderr, "Trying format 0x%4.4x\n", test_format);
#endif
        switch (test_format) {
        case AUDIO_U8:
            paspec.format = PA_SAMPLE_U8;
            break;
        case AUDIO_S16LSB:
            paspec.format = PA_SAMPLE_S16LE;
            break;
        case AUDIO_S16MSB:
            paspec.format = PA_SAMPLE_S16BE;
            break;
        default:
            paspec.format = PA_SAMPLE_INVALID;
            break;
        }
        if (paspec.format == PA_SAMPLE_INVALID) {
            test_format = SDL_NextAudioFormat();
        }
    }
    if (paspec.format == PA_SAMPLE_INVALID) {
        PULSEAUDIO_CloseDevice(this);
        SDL_SetError("Couldn't find any hardware audio formats");
        return 0;
    }
    this->spec.format = test_format;

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate mixing buffer */
    this->hidden->mixlen = this->spec.size;
    this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(this->hidden->mixlen);
    if (this->hidden->mixbuf == NULL) {
        PULSEAUDIO_CloseDevice(this);
        SDL_OutOfMemory();
        return 0;
    }
    SDL_memset(this->hidden->mixbuf, this->spec.silence, this->spec.size);

    paspec.channels = this->spec.channels;
    paspec.rate = this->spec.freq;

    /* Reduced prebuffering compared to the defaults. */
    paattr.tlength = this->hidden->mixlen;
    paattr.minreq = this->hidden->mixlen;
    paattr.fragsize = this->hidden->mixlen;
    paattr.prebuf = this->hidden->mixlen;
    paattr.maxlength = this->hidden->mixlen * 4;

    /* The SDL ALSA output hints us that we use Windows' channel mapping */
    /* http://bugzilla.libsdl.org/show_bug.cgi?id=110 */
    SDL_NAME(pa_channel_map_init_auto) (&pacmap, this->spec.channels,
                                        PA_CHANNEL_MAP_WAVEEX);

    /* Connect to the PulseAudio server */
    this->hidden->stream = SDL_NAME(pa_simple_new) (SDL_getenv("PASERVER"),     /* server */
                                                    get_progname(),     /* application name */
                                                    PA_STREAM_PLAYBACK, /* playback mode */
                                                    SDL_getenv("PADEVICE"),     /* device on the server */
                                                    "Simple DirectMedia Layer", /* stream description */
                                                    &paspec,    /* sample format spec */
                                                    &pacmap,    /* channel map */
                                                    &paattr,    /* buffering attributes */
                                                    &err        /* error code */
        );

    if (this->hidden->stream == NULL) {
        PULSEAUDIO_CloseDevice(this);
        SDL_SetError("Could not connect to PulseAudio: %s",
                     SDL_NAME(pa_strerror(err)));
        return 0;
    }

    /* Get the parent process id (we're the parent of the audio thread) */
    this->hidden->parent = getpid();

    /* We're ready to rock and roll. :-) */
    return 1;
}


static void
PULSEAUDIO_Deinitialize(void)
{
    UnloadPulseLibrary();
}


static int
PULSEAUDIO_Init(SDL_AudioDriverImpl * impl)
{
    if (LoadPulseLibrary() < 0) {
        return 0;
    }

    /* Set the function pointers */
    impl->OpenDevice = PULSEAUDIO_OpenDevice;
    impl->PlayDevice = PULSEAUDIO_PlayDevice;
    impl->WaitDevice = PULSEAUDIO_WaitDevice;
    impl->GetDeviceBuf = PULSEAUDIO_GetDeviceBuf;
    impl->CloseDevice = PULSEAUDIO_CloseDevice;
    impl->WaitDone = PULSEAUDIO_WaitDone;
    impl->Deinitialize = PULSEAUDIO_Deinitialize;
    impl->OnlyHasDefaultOutputDevice = 1;

    /* !!! FIXME: should test if server is available here, return 2 if so. */
    return 1;
}


AudioBootStrap PULSEAUDIO_bootstrap = {
    PULSEAUDIO_DRIVER_NAME, "PulseAudio", PULSEAUDIO_Init, 0
};

/* vi: set ts=4 sw=4 expandtab: */
