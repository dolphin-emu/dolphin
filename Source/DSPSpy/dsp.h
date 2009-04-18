#ifndef __DSP_H__
#define __DSP_H__

#include <gctypes.h>

#define DSPTASK_INIT		0
#define DSPTASK_RUN			1
#define DSPTASK_YIELD		2
#define DSPTASK_DONE		3

#define DSPTASK_CLEARALL	0x00000000
#define DSPTASK_ATTACH		0x00000001
#define DSPTASK_CANCEL		0x00000002

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*DSPCallback)(void *task);

typedef struct _dsp_task {
	vu32 state;
	vu32 prio;
	vu32 flags;
	
	u16 init_vec;
	u16 resume_vec;
	
	u16 *iram_maddr;
	u32 iram_len;
	u16 iram_addr;

	u16 *dram_maddr;
	u32 dram_len;
	u16 dram_addr;
	
	DSPCallback init_cb;
	DSPCallback res_cb;
	DSPCallback done_cb;
	DSPCallback req_cb;

	struct _dsp_task *next;
	struct _dsp_task *prev;
} dsptask_t;

void DSP_Init();
u32 DSP_CheckMailTo();
u32 DSP_CheckMailFrom();
u32 DSP_ReadMailFrom();
void DSP_AssertInt();
void DSP_SendMailTo(u32 mail);
u32 DSP_ReadCPUtoDSP();
dsptask_t* DSP_AddTask(dsptask_t *task);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
