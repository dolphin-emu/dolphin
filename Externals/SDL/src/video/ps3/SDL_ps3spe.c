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
#include "SDL_config.h"

#include "SDL_video.h"
#include "SDL_ps3spe_c.h"

#include "SDL_ps3video.h"
#include "SDL_ps3render_c.h"

/* Start the SPE thread */
int SPE_Start(spu_data_t * spe_data)
{
  deprintf(2, "[PS3->SPU] Start SPE: %s\n", spe_data->program_name);
  if (!(spe_data->booted))
    SPE_Boot(spe_data);

  /* To allow re-running of context, spe_ctx_entry has to be set before each call */
  spe_data->entry = SPE_DEFAULT_ENTRY;
  spe_data->error_code = 0;

  /* Create SPE thread and run */
  deprintf(2, "[PS3->SPU] Create Thread: %s\n", spe_data->program_name);
  if (pthread_create
      (&spe_data->thread, NULL, (void *)&SPE_RunContext, (void *)spe_data)) {
    deprintf(2, "[PS3->SPU] Could not create pthread for spe: %s\n", spe_data->program_name);
    SDL_SetError("[PS3->SPU] Could not create pthread for spe");
    return -1;
  }

  if (spe_data->keepalive)
    SPE_WaitForMsg(spe_data, SPU_READY);
}

/* Stop the SPE thread */
int SPE_Stop(spu_data_t * spe_data)
{
  deprintf(2, "[PS3->SPU] Stop SPE: %s\n", spe_data->program_name);
  /* Wait for SPE thread to complete */
  deprintf(2, "[PS3->SPU] Wait for SPE thread to complete: %s\n", spe_data->program_name);
  if (pthread_join(spe_data->thread, NULL)) {
    deprintf(2, "[PS3->SPU] Failed joining the thread: %s\n", spe_data->program_name);
    SDL_SetError("[PS3->SPU] Failed joining the thread");
    return -1;
  }

  return 0;
}

/* Create SPE context and load program */
int SPE_Boot(spu_data_t * spe_data)
{
  /* Create SPE context */
  deprintf(2, "[PS3->SPU] Create SPE Context: %s\n", spe_data->program_name);
  spe_data->ctx = spe_context_create(0, NULL);
  if (spe_data->ctx == NULL) {
    deprintf(2, "[PS3->SPU] Failed creating SPE context: %s\n", spe_data->program_name);
    SDL_SetError("[PS3->SPU] Failed creating SPE context");
    return -1;
  }

  /* Load SPE object into SPE local store */
  deprintf(2, "[PS3->SPU] Load Program into SPE: %s\n", spe_data->program_name);
  if (spe_program_load(spe_data->ctx, &spe_data->program)) {
    deprintf(2, "[PS3->SPU] Failed loading program into SPE context: %s\n", spe_data->program_name);
    SDL_SetError
        ("[PS3->SPU] Failed loading program into SPE context");
    return -1;
  }
  spe_data->booted = 1;
  deprintf(2, "[PS3->SPU] SPE boot successful\n");

  return 0;
}

/* (Stop and) shutdown the SPE */
int SPE_Shutdown(spu_data_t * spe_data)
{
  if (spe_data->keepalive && spe_data->booted) {
    SPE_SendMsg(spe_data, SPU_EXIT);
    SPE_Stop(spe_data);
  }

  /* Destroy SPE context */
  deprintf(2, "[PS3->SPU] Destroy SPE context: %s\n", spe_data->program_name);
  if (spe_context_destroy(spe_data->ctx)) {
    deprintf(2, "[PS3->SPU] Failed destroying context: %s\n", spe_data->program_name);
    SDL_SetError("[PS3->SPU] Failed destroying context");
    return -1;
  }
  deprintf(2, "[PS3->SPU] SPE shutdown successful: %s\n", spe_data->program_name);
  return 0;
}

/* Send message to the SPE via mailboxe */
int SPE_SendMsg(spu_data_t * spe_data, unsigned int msg)
{
  deprintf(2, "[PS3->SPU] Sending message %u to %s\n", msg, spe_data->program_name);
  /* Send one message, block until message was sent */
  unsigned int spe_in_mbox_msgs[1];
  spe_in_mbox_msgs[0] = msg;
  int in_mbox_write = spe_in_mbox_write(spe_data->ctx, spe_in_mbox_msgs, 1, SPE_MBOX_ALL_BLOCKING);

  if (1 > in_mbox_write) {
    deprintf(2, "[PS3->SPU] No message could be written to %s\n", spe_data->program_name);
    SDL_SetError("[PS3->SPU] No message could be written");
    return -1;
  }
  return 0;
}


/* Read 1 message from SPE, block until at least 1 message was received */
int SPE_WaitForMsg(spu_data_t * spe_data, unsigned int msg)
{
  deprintf(2, "[PS3->SPU] Waiting for message from %s\n", spe_data->program_name);
  unsigned int out_messages[1];
  while (!spe_out_mbox_status(spe_data->ctx));
  int mbox_read = spe_out_mbox_read(spe_data->ctx, out_messages, 1);
  deprintf(2, "[PS3->SPU] Got message from %s, message was %u\n", spe_data->program_name, out_messages[0]);
  if (out_messages[0] == msg)
    return 0;
  else
    return -1;
}

/* Re-runnable invocation of the spe_context_run call */
void SPE_RunContext(void *thread_argp)
{
  /* argp is the pointer to argument to be passed to the SPE program */
  spu_data_t *args = (spu_data_t *) thread_argp;
  deprintf(3, "[PS3->SPU] void* argp=0x%x\n", (unsigned int)args->argp);

  /* Run it.. */
  deprintf(2, "[PS3->SPU] Run SPE program: %s\n", args->program_name);
  if (spe_context_run
      (args->ctx, &args->entry, 0, (void *)args->argp, NULL,
       NULL) < 0) {
    deprintf(2, "[PS3->SPU] Failed running SPE context: %s\n", args->program_name);
    SDL_SetError("[PS3->SPU] Failed running SPE context: %s", args->program_name);
    exit(1);
  }

  pthread_exit(NULL);
}

/* vi: set ts=4 sw=4 expandtab: */
