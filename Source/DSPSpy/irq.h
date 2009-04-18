#ifndef __IRQ_H__
#define __IRQ_H__

#include <gctypes.h>
#include "context.h"

#define IM_NONE				 (0x00000000)
#define IRQ_MEM0			 0
#define IRQ_MEM1			 1
#define IRQ_MEM2			 2
#define IRQ_MEM3			 3
#define IRQ_MEMADDRESS		 4
#define IRQ_DSP_AI			 5
#define IRQ_DSP_ARAM		 6
#define IRQ_DSP_DSP			 7
#define IRQ_AI_AI			 8
#define IRQ_EXI0_EXI		 9
#define IRQ_EXI0_TC			 10
#define IRQ_EXI0_EXT		 11
#define IRQ_EXI1_EXI		 12
#define IRQ_EXI1_TC			 13
#define IRQ_EXI1_EXT		 14
#define IRQ_EXI2_EXI		 15
#define IRQ_EXI2_TC			 16
#define IRQ_PI_CP			 17
#define IRQ_PI_PETOKEN		 18
#define IRQ_PI_PEFINISH		 19
#define IRQ_PI_SI			 20
#define IRQ_PI_DI			 21
#define IRQ_PI_RSW			 22
#define IRQ_PI_ERROR		 23
#define IRQ_PI_VI			 24
#define IRQ_PI_DEBUG		 25
#define IRQ_PI_HSP			 26
#define IRQ_MAX				 32

#define IRQMASK(irq)		 (0x80000000u>>irq)

#define IM_MEM0				 IRQMASK(IRQ_MEM0)
#define IM_MEM1				 IRQMASK(IRQ_MEM1)
#define IM_MEM2				 IRQMASK(IRQ_MEM2)
#define IM_MEM3				 IRQMASK(IRQ_MEM3)
#define IM_MEMADDRESS		 IRQMASK(IRQ_MEMADDRESS)
#define IM_MEM				 (IM_MEM0|IM_MEM1|IM_MEM2|IM_MEM3|IM_MEMADDRESS)

#define IM_DSP_AI			 IRQMASK(IRQ_DSP_AI)
#define IM_DSP_ARAM			 IRQMASK(IRQ_DSP_ARAM)
#define IM_DSP_DSP			 IRQMASK(IRQ_DSP_DSP)
#define IM_DSP				 (IM_DSP_AI|IM_DSP_ARAM|IM_DSP_DSP)

#define IM_AI_AI			 IRQMASK(IRQ_AI_AI)
#define IM_AI				 (IRQ_AI_AI)

#define IM_EXI0_EXI			 IRQMASK(IRQ_EXI0_EXI)
#define IM_EXI0_TC			 IRQMASK(IRQ_EXI0_TC)
#define IM_EXI0_EXT			 IRQMASK(IRQ_EXI0_EXT)
#define IM_EXI0				 (IM_EXI0_EXI|IM_EXI0_TC|IM_EXI0_EXT)

#define IM_EXI1_EXI			 IRQMASK(IRQ_EXI1_EXI)
#define IM_EXI1_TC			 IRQMASK(IRQ_EXI1_TC)
#define IM_EXI1_EXT			 IRQMASK(IRQ_EXI1_EXT)
#define IM_EXI1				 (IM_EXI1_EXI|IM_EXI1_TC|IM_EXI1_EXT)

#define IM_EXI2_EXI			 IRQMASK(IRQ_EXI2_EXI)
#define IM_EXI2_TC			 IRQMASK(IRQ_EXI2_TC)
#define IM_EXI2				 (IM_EXI2_EXI|IM_EXI2_TC)
#define IM_EXI				 (IM_EXI0|IM_EXI1|IM_EXI2)

#define IM_PI_CP			 IRQMASK(IRQ_PI_CP)
#define IM_PI_PETOKEN		 IRQMASK(IRQ_PI_PETOKEN)
#define IM_PI_PEFINISH		 IRQMASK(IRQ_PI_PEFINISH)
#define IM_PI_SI			 IRQMASK(IRQ_PI_SI)
#define IM_PI_DI			 IRQMASK(IRQ_PI_DI)
#define IM_PI_RSW			 IRQMASK(IRQ_PI_RSW)
#define IM_PI_ERROR			 IRQMASK(IRQ_PI_ERROR)
#define IM_PI_VI			 IRQMASK(IRQ_PI_VI)
#define IM_PI_DEBUG			 IRQMASK(IRQ_PI_DEBUG)
#define IM_PI_HSP			 IRQMASK(IRQ_PI_HSP)
#define IM_PI				 (IM_PI_CP|IM_PI_PETOKEN|IM_PI_PEFINISH|IM_PI_SI|IM_PI_DI|IM_PI_RSW|IM_PI_ERROR|IM_PI_VI|IM_PI_DEBUG|IM_PI_HSP)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (raw_irq_handler_t)(u32,void *);

raw_irq_handler_t* IRQ_Request(u32 nIrq,raw_irq_handler_t *pHndl,void *pCtx);
raw_irq_handler_t* IRQ_Free(u32 nIrq);
raw_irq_handler_t* IRQ_GetHandler(u32 nIrq);
u32 IRQ_Disable();
void IRQ_Restore(u32 level);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
