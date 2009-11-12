/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org

    QNX Sound Architecture (QSA) SDL driver
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include "SDL_config.h"

#ifndef __SDL_QSA_AUDIO_H__
#define __SDL_QSA_AUDIO_H__

#include <sys/asoundlib.h>

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice* this

struct SDL_PrivateAudioData
{
    /* SDL capture state */
    int iscapture;

    /* The audio device handle */
    int cardno;
    int deviceno;
    snd_pcm_t *audio_handle;

    /* The audio file descriptor */
    int audio_fd;

    /* Select timeout status */
    uint32_t timeout_on_wait;

    /* Raw mixing buffer */
    Uint8 *pcm_buf;
    Uint32 pcm_len;
};

#endif /* __SDL_QSA_AUDIO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
