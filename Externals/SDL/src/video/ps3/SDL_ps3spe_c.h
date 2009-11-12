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

/* This SPE API basically provides 3 ways to run and control a program
 * on the SPE:
 * - Start and stop the program (keepalive=0).
 *   SPE_Start() will implicitly boot up the program, create a thread and run
 *   the context.
 *   SPE_Stop() will join the (terminated) thread (may block) and return.
 * - Boot the program and run it (keepalive=0).
 *   SPE_Boot() will create a context and load the program and finally start
 *   the context with SPE_Start().
 *   SPE_Stop() will savely end the program.
 * - Boot, Run and send messages to the program (keepalive=1).
 *   Start the program by using one of the methods described above. When
 *   received the READY-message the program is in its infinite loop waiting
 *   for new messages.
 *   Every time you run the program, send SPU_START and the address of the
 *   according struct using SPE_SendMsg().
 *   SPE_WaitForMsg() will than wait for SPU_FIN and is blocking.
 *   SPE_Shutdown() sends SPU_EXIT and finally stops the program.
 *
 * Therefor the SPE program
 * - either runs once and returns
 * - or runs in an infinite loop and is controlled by messages.
 */

#include "SDL_config.h"

#include "spulibs/spu_common.h"

#include <libspe2.h>

#ifndef _SDL_ps3spe_h
#define _SDL_ps3spe_h

/* SPU handling data */
typedef struct spu_data {
    /* Context to be executed */
    spe_context_ptr_t ctx;
    spe_program_handle_t program;
    /* Thread running the context */
    pthread_t thread;
    /* For debugging */
    char * program_name;
    /* SPE_Start() or SPE_Boot() called */
    unsigned int booted;
    /* Runs the program in an infinite loop? */
    unsigned int keepalive;
    unsigned int entry;
    /* Exit code of the program */
    int error_code;
    /* Arguments passed to the program */
    void * argp;
} spu_data_t;

/* SPU specific API functions */
int SPE_Start(spu_data_t * spe_data);
int SPE_Stop(spu_data_t * spe_data);
int SPE_Boot(spu_data_t * spe_data);
int SPE_Shutdown(spu_data_t * spe_data);
int SPE_SendMsg(spu_data_t * spe_data, unsigned int msg);
int SPE_WaitForMsg(spu_data_t * spe_data, unsigned int msg);
void SPE_RunContext(void *thread_argp);

#endif /* _SDL_ps3spe_h */

/* vi: set ts=4 sw=4 expandtab: */
