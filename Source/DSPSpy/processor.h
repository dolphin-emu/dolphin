#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include <gctypes.h>

#define __stringify(rn)					#rn
#define ATTRIBUTE_ALIGN(v)				__attribute__((aligned(v)))

#define ppcsync() asm volatile("sc")
#define ppchalt() ({					\
	asm volatile("sync");				\
	while(1) {							\
		asm volatile("nop");			\
		asm volatile("li 3,0");			\
		asm volatile("nop");			\
	}									\
})

#define mfdcr(_rn) ({register u32 _rval; \
		asm volatile("mfdcr %0," __stringify(_rn) \
             : "=r" (_rval)); _rval;})
#define mtdcr(rn, val)  asm volatile("mtdcr " __stringify(rn) ",%0" : : "r" (val))

#define mfmsr()   ({register u32 _rval; \
		asm volatile("mfmsr %0" : "=r" (_rval)); _rval;})
#define mtmsr(val)  asm volatile("mtmsr %0" : : "r" (val))

#define mfdec()   ({register u32 _rval; \
		asm volatile("mfdec %0" : "=r" (_rval)); _rval;})
#define mtdec(_val)  asm volatile("mtdec %0" : : "r" (_val))

#define mfspr(_rn) ({register u32 _rval; \
		asm volatile("mfspr %0," __stringify(_rn) \
             : "=r" (_rval)); _rval;})
#define mtspr(_rn, _val)  asm volatile("mtspr " __stringify(_rn) ",%0" : : "r" (_val))

#define mfwpar()		mfspr(921)
#define mtwpar(_val)	mtspr(921,_val)

#define mfmmcr0()		mfspr(952)
#define mtmmcr0(_val)	mtspr(952,_val)
#define mfmmcr1()		mfspr(956)
#define mtmmcr1(_val)	mtspr(956,_val)

#define mfpmc1()		mfspr(953)
#define mtpmc1(_val)	mtspr(953,_val)
#define mfpmc2()		mfspr(954)
#define mtpmc2(_val)	mtspr(954,_val)
#define mfpmc3()		mfspr(957)
#define mtpmc3(_val)	mtspr(957,_val)
#define mfpmc4()		mfspr(958)
#define mtpmc4(_val)	mtspr(958,_val)

#define cntlzw(_val) ({register u32 _rval; \
					  asm volatile("cntlzw %0, %1" : "=r"((_rval)) : "r"((_val))); _rval;})

#define _CPU_MSR_GET( _msr_value ) \
  do { \
    _msr_value = 0; \
    asm volatile ("mfmsr %0" : "=&r" ((_msr_value)) : "0" ((_msr_value))); \
  } while (0)

#define _CPU_MSR_SET( _msr_value ) \
{ asm volatile ("mtmsr %0" : "=&r" ((_msr_value)) : "0" ((_msr_value))); }

#define _CPU_ISR_Enable() \
	{ register u32 _val = 0; \
	  asm volatile ("mfmsr %0; ori %0,%0,0x8000; mtmsr %0" : \
					"=&r" (_val) : "0" (_val));\
	}

#define _CPU_ISR_Disable( _isr_cookie ) \
  { register u32 _disable_mask = MSR_EE; \
    _isr_cookie = 0; \
    asm volatile ( \
	"mfmsr %0; andc %1,%0,%1; mtmsr %1" : \
	"=&r" ((_isr_cookie)), "=&r" ((_disable_mask)) : \
	"0" ((_isr_cookie)), "1" ((_disable_mask)) \
	); \
  }

#define _CPU_ISR_Restore( _isr_cookie )  \
  { \
     asm volatile ( "mtmsr %0" : \
		   "=r" ((_isr_cookie)) : \
                   "0" ((_isr_cookie))); \
  }

#define _CPU_ISR_Flash( _isr_cookie ) \
  { register u32 _disable_mask = MSR_EE; \
    asm volatile ( \
      "mtmsr %0; andc %1,%0,%1; mtmsr %1" : \
      "=r" ((_isr_cookie)), "=r" ((_disable_mask)) : \
      "0" ((_isr_cookie)), "1" ((_disable_mask)) \
    ); \
  }

#endif
