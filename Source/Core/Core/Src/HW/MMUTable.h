#ifndef MMUTable_H
#define MMUTable_H

#include "CommonTypes.h"
#include "Atomic.h"
#include "../PowerPC/PowerPC.h"


#define MMUTABLE_CHECK
//#define MMU_ALL_READ_WARNINGS
//#define MMU_ALL_WRITE_WARNINGS

#define MMU_ON_MAP_ALL_WARN

//#define  MMU_ON_MSR_CHANGE_WARNING
//#define MMU_ON_IBAT_CHANGE_WARNING
//#define MMU_ON_DBAT_CHANGE_WARNING
//#define MMU_READ32_EXEC_WARNING
#define MMU_ON_SR_CHANGE_WARNING
#define MMU_ON_SDR_CHANGE_WARNING
//#define MMU_ON_PAGETABLE_READ_WARNING
//#define MMU_ON_PAGETABLE_WRITE_WARNING
#define MMU_ON_ERROR_READ_WARN
#define MMU_ON_ERROR_WRITE_WARN

#ifdef MMU_ALL_READ_WARNINGS
#define  MMU_READ32_EXEC_WARNING
#define  MMU_READ8_WARNING
#define  MMU_READ16_WARNING
#define  MMU_READ32_WARNING
#define  MMU_READ64_WARNING
#endif

#ifdef MMU_ALL_WRITE_WARNINGS
#define  MMU_WRITE8_WARNING
#define  MMU_WRITE16_WARNING
#define  MMU_WRITE32_WARNING
#define  MMU_WRITE64_WARNING
#endif


#ifdef MMU_ON_MAP_ALL_WARN
#define MMU_ON_MAP_BAT_WARN
#define MMU_ON_MAP_PTE_WARN
#define MMU_ON_UNMAP_PTE_WARN
#endif




#define PP_NOACCESS(pp) (pp==0)
#define PP_RW(pp) (pp==2)
#define PP_RO(pp) (pp&1)
#define PTEH_VALID 0x80000000
#define SR_OPTION_Ks (0x40000000)
#define SR_OPTION_Kp (0x20000000)
#define SR_OPTION_N (0x10000000)
#define ACCESS_MASK_PR 0x4
#define ACCESS_MASK_PR_USER 0x4
#define ACCESS_MASK_PR_SUPER 0x0
#define ACCESS_MASK_IR 0x2
#define ACCESS_MASK_IR_ON 0x2
#define ACCESS_MASK_IR_OFF 0x0
#define ACCESS_MASK_DR 0x1
#define ACCESS_MASK_DR_ON 0x1
#define ACCESS_MASK_DR_OFF 0x0
#define ACCESS_COUNT 8
#define ACCESS_MASK_PHYSICAL 0


#define PPC_EXC_DSISR_PAGE (1<<30)
#define PPC_EXC_DSISR_PROT (1<<27)
#define PPC_EXC_DSISR_STORE (1<<25) 



namespace Memory
{
void GenerateDSIException(u32 _EffectiveAdress, bool _bWrite);
void GenerateISIException(u32 _EffectiveAdress);
void GenerateDSIExceptionEx(u32 _EffectiveAddress, u32 bits);
void GenerateISIExceptionEx(u32 _EffectiveAddress, u32 bits);
}

namespace MMUTable
{

const int DSI_TRANSLATE=-1;
const int DSI_PROT=-2;

const int ISI_TRANSLATE=-1;
const int ISI_PROT=-2;
const int ISI_N=-3;

struct EmuPointer
{
	u32 m_addr;

	explicit EmuPointer(u32 addr): m_addr(addr) {}
	EmuPointer &operator+=(int val) { m_addr += val; return *this;}
	EmuPointer &operator++(int val) { m_addr++; return *this;}
	EmuPointer &operator--(int val) { m_addr--; return *this;}
};

struct DAccessFuncs
{
	int (*read_u8)(const void* context, const u32 addr, u8 &out);
	int (*read_u16)(const void* context, const u32 addr, u16 &out);
	int (*read_u32)(const void* context, const u32 addr, u32 &out);
	int (*read_u64)(const void* context, const u32 addr, u64 &out);
	int (*write_u8)(void* context, const u32 addr, const u8 in);
	int (*write_u16)(void* context, const u32 addr, const u16 in);
	int (*write_u32)(void* context, const u32 addr, const u32 in);
	int (*write_u64)(void* context, const u32 addr, const u64 in);

	bool operator==(const DAccessFuncs &other)
	{
		if(read_u8 != other.read_u8) return false;
		if(read_u16 != other.read_u16) return false;
		if(read_u32 != other.read_u32) return false;
		if(read_u64 != other.read_u64) return false;
		if(write_u8 != other.write_u8) return false;
		if(write_u16 != other.write_u16) return false;
		if(write_u32 != other.write_u32) return false;
		if(write_u64 != other.write_u64) return false;
		return true;
	}
};
struct IAccessFuncs
{
	int (*read_u32_exec)(const void* context, const u32 addr, u32 &out);
};

struct MMUMappingAccess
{
	DAccessFuncs daf;
	IAccessFuncs iaf;
	void *contextd;
	void *contexti;
	u32 typei;
	u32 typed;
};


void reset();
void daf_reset_to_no_except(DAccessFuncs *daf);
void map_mmio_device(DAccessFuncs *daf, void *contextd, u32 phys_size, u32 phys_addr);
void map_physical(u8 *real_base, u32 phys_size, u32 phys_addr);
void on_ibatl_change(int index, u32 ibatu_value, u32 ibatl_value);
void on_ibatu_change(int index, u32 ibatu_old_value, u32 ibatu_new_value, u32 ibatl_value);
void on_dbatl_change(int index, u32 dbatu_value, u32 dbatl_value);
void on_dbatu_change(int index, u32 dbatu_old_value, u32 dbatu_new_value, u32 dbatl_value);
void on_msr_change();
void on_sr_change(int sr, u32 new_value);
void on_sdr_change(u32 new_value);

extern u32 access_mask;
extern MMUMappingAccess memory_access[ACCESS_COUNT][0x100000];


inline u32 get_access_mask()
{
	return access_mask;
}

inline void resync_access_mask()
{
	UReg_MSR& msr = (UReg_MSR&)MSR;
	access_mask = (msr.PR<<2) | (msr.IR <<1) | (msr.DR);
}

/*
int read_instr(const EmuPointer addr, u32 &out, u32 am=get_access_mask());
int read8(const EmuPointer addr, u8 &out, u32 am=get_access_mask());
int read16(const EmuPointer addr, u16 &out, u32 am=get_access_mask());
int read32(const EmuPointer addr, u32 &out, u32 am=get_access_mask());
int read64(const EmuPointer addr, u64 &out, u32 am=get_access_mask());
int write8(const EmuPointer addr, const u8 in, u32 am=get_access_mask());
int write16(const EmuPointer addr, const u16 in, u32 am=get_access_mask());
int write32(const EmuPointer addr, const u32 in, u32 am=get_access_mask());
int write64(const EmuPointer addr, const u64 in, u32 am=get_access_mask());


int read_instr_ne(const EmuPointer addr, u32 &out, u32 am=get_access_mask());
int read8_ne(const EmuPointer addr, u8 &out, u32 am=get_access_mask());
int read16_ne(const EmuPointer addr, u16 &out, u32 am=get_access_mask());
int read32_ne(const EmuPointer addr, u32 &out, u32 am=get_access_mask());
int read64_ne(const EmuPointer addr, u64 &out, u32 am=get_access_mask());
int write8_ne(const EmuPointer addr, const u8 in, u32 am=get_access_mask());
int write16_ne(const EmuPointer addr, const u16 in, u32 am=get_access_mask());
int write32_ne(const EmuPointer addr, const u32 in, u32 am=get_access_mask());
int write64_ne(const EmuPointer addr, const u64 in, u32 am=get_access_mask());
*/
static inline int read_instr_ne(const EmuPointer addr, u32 &out, u32 am=get_access_mask())
{
	const void *contexti = memory_access[access_mask][addr.m_addr>>12].contexti;
	return memory_access[am][addr.m_addr>>12].iaf.read_u32_exec(contexti, addr.m_addr, out);
} 

static inline int read_instr(const EmuPointer addr, u32 &out, u32 am=get_access_mask())
{
	int rv = read_instr_ne(addr, out, am);
	if(rv < 0)
	{
		u32 ISISR_bits = 0; 
		switch(rv)
		{
			case ISI_TRANSLATE:
				ISISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case ISI_PROT:
				ISISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateISIExceptionEx(addr.m_addr, ISISR_bits);
#ifdef MMU_READ32_EXEC_WARNING
i		WARN_LOG(MASTER_LOG, "Program read_instr ISI[%08x] rv=%d [PC=0x%08x, LR=0x%08x]", addr.m_addr, rv, PowerPC::ppcState.pc, PowerPC::ppcState.spr[SPR_LR]);
#endif
	}
	return rv;
} 

static inline int read8_ne(const EmuPointer addr, u8 &out, u32 am=get_access_mask())
{
	const void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.read_u8(contextd, addr.m_addr, out);
}

static inline int read8(const EmuPointer addr, u8 &out, u32 am=get_access_mask())
{
	int rv = read8_ne(addr, out, am);
	if(rv < 0)
	{
		u32 DSISR_bits = 0; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_READ8_WARNING
		WARN_LOG(MASTER_LOG, "Program read8 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
} 

static inline int read16_ne(const EmuPointer addr, u16 &out, u32 am=get_access_mask())
{
	const void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.read_u16(contextd, addr.m_addr, out);
} 

static inline int read16(const EmuPointer addr, u16 &out, u32 am=get_access_mask())
{
	int rv = read16_ne(addr, out, am); 
	if(rv < 0)
	{
		u32 DSISR_bits = 0; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_READ16_WARNING
		WARN_LOG(MASTER_LOG, "Program read16 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
} 

static inline int read32_ne(const EmuPointer addr, u32 &out, u32 am=get_access_mask())
{
	const void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	int rv= memory_access[am][addr.m_addr>>12].daf.read_u32(contextd, addr.m_addr, out);
	return rv;
} 


static inline int read32(const EmuPointer addr, u32 &out, u32 am=get_access_mask())
{
	int rv = read32_ne(addr, out, am);
	if(rv < 0)
	{
		u32 DSISR_bits = 0; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_READ32_WARNING
		WARN_LOG(MASTER_LOG, "Program read32 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
}

static inline int read64_ne(const EmuPointer addr, u64 &out, u32 am=get_access_mask())
{
	const void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.read_u64(contextd, addr.m_addr, out);
} 

static inline int read64(const EmuPointer addr, u64 &out, u32 am=get_access_mask())
{
	int rv = read64_ne(addr, out, am);
	if(rv < 0)
	{
		u32 DSISR_bits = 0; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_READ64_WARNING
		WARN_LOG(MASTER_LOG, "Program read64 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
} 

static inline int write8_ne(const EmuPointer addr, const u8 in, u32 am=get_access_mask())
{
	void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.write_u8(contextd, addr.m_addr, in);
}

static inline int write8(const EmuPointer addr, const u8 in, u32 am=get_access_mask())
{
	int rv = write8_ne(addr, in, am);
	if(rv < 0)
	{
		u32 DSISR_bits = PPC_EXC_DSISR_STORE; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_WRITE8_WARNING
		WARN_LOG(MASTER_LOG, "Program write8 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
} 
static inline int write16_ne(const EmuPointer addr, const u16 in, u32 am=get_access_mask())
{
	void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.write_u16(contextd, addr.m_addr, in);
}
static inline int write16(const EmuPointer addr, const u16 in, u32 am=get_access_mask())
{
	int rv = write16_ne(addr, in, am);
	if(rv < 0)
	{
		u32 DSISR_bits = PPC_EXC_DSISR_STORE; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_WRITE16_WARNING
		WARN_LOG(MASTER_LOG, "Program write16 DSI[%08x] rv=%d", addr.m_addr, rv);
#endif
	}
	return rv;
} 

static inline int write32_ne(const EmuPointer addr, const u32 in, u32 am=get_access_mask())
{
	void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.write_u32(contextd, addr.m_addr, in);
}
static inline int write32(const EmuPointer addr, const u32 in, u32 am=get_access_mask())
{
	int rv = write32_ne(addr, in, am);
	if(rv < 0)
	{
		u32 DSISR_bits = PPC_EXC_DSISR_STORE; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_WRITE32_WARNING
		WARN_LOG(MASTER_LOG, "Program write32 DSI[%08x] rv=%d PC=%08x", addr.m_addr, rv, PowerPC::ppcState.pc);
#endif
	}
	return rv;
} 

static inline int write64_ne(const EmuPointer addr, const u64 in, u32 am=get_access_mask())
{
	void *contextd = memory_access[am][addr.m_addr>>12].contextd;
	return memory_access[am][addr.m_addr>>12].daf.write_u64(contextd, addr.m_addr, in);
}
static inline int write64(const EmuPointer addr, const u64 in, u32 am=get_access_mask())
{
	int rv = write64_ne(addr, in, am);
	if(rv < 0)
	{
		u32 DSISR_bits = PPC_EXC_DSISR_STORE; 
		switch(rv)
		{
			case DSI_TRANSLATE:
				DSISR_bits |= PPC_EXC_DSISR_PAGE;
				break;
			case DSI_PROT:
				DSISR_bits |= PPC_EXC_DSISR_PROT;
				break;
				
		}
		Memory::GenerateDSIExceptionEx(addr.m_addr, DSISR_bits);
#ifdef MMU_WRITE64_WARNING
		WARN_LOG(MASTER_LOG, "Program write64 DSI[%08x] {%016llx} rv=%d", addr.m_addr, in, rv);
#endif
	}
	return rv;
} 



u8 *get_physical_addr_pointer(const EmuPointer &addr);

//void *memcpy_emu_to_real(void *dst, EmuPointer src, size_t n);
//EmuPointer memcpy_real_to_emu(EmuPointer dst, const void *src, size_t n);
EmuPointer memset_emu(EmuPointer s, int c, size_t n);


} // MMUTable Namespace

#endif
