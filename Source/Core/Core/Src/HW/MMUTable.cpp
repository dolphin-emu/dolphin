#include "CommonTypes.h"
#include "MMUTable.h"
//#include "../PowerPC/Gekko.h"



#include <stdio.h>


namespace MMUTable
{

const int PAGE_SIZE=4096;
const int PAGE_MASK=PAGE_SIZE-1;

const u32 MAP_PAGE_NORMAL=0;
const u32 MAP_PAGE_NO_C=1;
const u32 MAP_PAGE_NO_RC=2;
const u32 MAP_PAGE_HISTORY_MASK = 0x3;


const u32 MAP_TYPE_PTE=4;
const u32 MAP_TYPE_BAT=8;
const u32 MAP_TYPE_MASK=0xc;

MMUMappingAccess memory_access[ACCESS_COUNT][0x100000];


template<typename ACCESS_TYPE>
 int write_sz_ne(const EmuPointer addr, const ACCESS_TYPE in);

template <>
 int write_sz_ne(const EmuPointer addr, const u8 in)
{
	return write8_ne(addr, in);
}
template <>
 int write_sz_ne(const EmuPointer addr, const u16 in)
{
	return write16_ne(addr, in);
}
template <>
 int write_sz_ne(const EmuPointer addr, const u32 in)
{
	return write32_ne(addr, in);
}
template <>
 int write_sz_ne(const EmuPointer addr, const u64 in)
{
	return write64_ne(addr, in);
}
template<typename ACCESS_TYPE>
 int read_sz_ne(const EmuPointer addr, ACCESS_TYPE &out);

template <>
 int read_sz_ne(const EmuPointer addr, u8 &out)
{
	return read8_ne(addr, out);
}
template <>
 int read_sz_ne(const EmuPointer addr, u16 &out)
{
	return read16_ne(addr, out);
}
template <>
 int read_sz_ne(const EmuPointer addr, u32 &out)
{
	return read32_ne(addr, out);
}
template <>
 int read_sz_ne(const EmuPointer addr, u64 &out)
{
	return read64_ne(addr, out);
}


template<typename ACCESS_TYPE, int type>
int trigger_read_exception(const void* context, const u32 addr, ACCESS_TYPE &val)
{
	return type;
}

template<typename ACCESS_TYPE, int type>
int trigger_write_exception(void* context, const u32 addr, const ACCESS_TYPE val)
{
	return type;
}

static struct IAccessFuncs INoTranslation =
{
	&trigger_read_exception<u32, ISI_TRANSLATE>,
};

static struct DAccessFuncs DNoTranslation =
{
	&trigger_read_exception<u8, DSI_TRANSLATE>,
	&trigger_read_exception<u16, DSI_TRANSLATE>,
	&trigger_read_exception<u32, DSI_TRANSLATE>,
	&trigger_read_exception<u64, DSI_TRANSLATE>,
	&trigger_write_exception<u8, DSI_TRANSLATE>,
	&trigger_write_exception<u16, DSI_TRANSLATE>,
	&trigger_write_exception<u32, DSI_TRANSLATE>,
	&trigger_write_exception<u64, DSI_TRANSLATE>,
};


template <typename T> T swapper(T in);

template <> u8 swapper(u8 in)
{
	return in;
}

template <> u16 swapper(u16 in)
{
	return Common::swap16(in);
}

template <> u32 swapper(u32 in)
{
	return Common::swap32(in);
}

template <> u64 swapper(u64 in)
{
	return Common::swap64(in);
}

template <typename ACCESS_TYPE>
int read_via_pt_no_r(const void* context, const u32 addr, ACCESS_TYPE &out)
{
	u32 vsid = PowerPC::ppcState.sr[addr>>28] & 0xffffff;
	u32 xor_out = (PowerPC::ppcState.sr[addr>>28] ^ ((addr >> 12) & 0xffff)) & (0x3ff | (PowerPC::ppcState.spr[SPR_SDR] & 0x1ff) << 10);
	u32 xor2_out = (~xor_out)&(0x3ff | (PowerPC::ppcState.spr[SPR_SDR] & 0x1ff) << 10);
	u32 pteg_addr  = (PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000) | (xor_out<<6);
	int i;
	u32 pte_h, pte_l;

	// check primary
	for(i=0; i<8; i++)
	{
		if(
			read32_ne(EmuPointer(pteg_addr+i*8), pte_h, ACCESS_MASK_PHYSICAL) ||
			read32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL)
		) continue;

		if((0x80000000 | (vsid << 7) | ((addr & 0xfc00000)>>22)) == pte_h) break;
	}

	if(i != 8) //primary hit
	{
		pte_l |= 0x100; //set r
//		WARN_LOG(MASTER_LOG, "setting R bit in %08x because of %08x[P]", pteg_addr+i*8, addr);
		write32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL);
		return read_sz_ne(EmuPointer(addr), out);
	}

	// check secondary
	pteg_addr  = (PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000) | (xor2_out<<6);

	for(i=0; i<8; i++)
	{
		if(
			read32_ne(EmuPointer(pteg_addr+i*8), pte_h, ACCESS_MASK_PHYSICAL) ||
			read32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL)
		) continue;

		if((0x80000000 | (vsid << 7) | 0x40 | ((addr & 0xfc00000)>>22)) == pte_h) break;
	}

	if(i != 8) //secondary hit
	{
		pte_l |= 0x100; //set r
//		WARN_LOG(MASTER_LOG, "setting R bit in %08x because of %08x[S]", pteg_addr+i*8, addr);
		write32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL);
		return read_sz_ne(EmuPointer(addr), out);
	}

	// problem! couldn't find the matching pte
	// do something smarter here
	WARN_LOG(MASTER_LOG, "COULD NOT FIND PTE!!!!: %08x", addr);
	return read_sz_ne(EmuPointer(addr), out);
}


template <typename ACCESS_TYPE>
int write_via_pt_no_c(void* context, const u32 addr, ACCESS_TYPE in)
{
	u32 vsid = PowerPC::ppcState.sr[addr>>28] & 0xffffff;
	u32 xor_out = (PowerPC::ppcState.sr[addr>>28] ^ ((addr >> 12) & 0xffff)) & (0x3ff | (PowerPC::ppcState.spr[SPR_SDR] & 0x1ff) << 10);
	u32 xor2_out = (~xor_out)&(0x3ff | (PowerPC::ppcState.spr[SPR_SDR] & 0x1ff) << 10);
	u32 pteg_addr  = (PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000) | (xor_out<<6);
	int i;
	u32 pte_h, pte_l;

	// check primary
	for(i=0; i<8; i++)
	{
		if(
			read32_ne(EmuPointer(pteg_addr+i*8), pte_h, ACCESS_MASK_PHYSICAL) ||
			read32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL)
		) continue;

		if((0x80000000 | (vsid << 7) | ((addr & 0xfc00000)>>22)) == pte_h) break;
	}

	if(i != 8) //primary hit
	{
		pte_l |= 0x180; //set rc
//		WARN_LOG(MASTER_LOG, "setting RC bits in %08x because of %08x[P]", pteg_addr+i*8, addr);
		write32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL);
		return write_sz_ne(EmuPointer(addr), in);
	}

	// check secondary
	pteg_addr  = (PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000) | (xor2_out<<6);

	for(i=0; i<8; i++)
	{
		if(
			read32_ne(EmuPointer(pteg_addr+i*8), pte_h, ACCESS_MASK_PHYSICAL) ||
			read32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL)
		) continue;

		if((0x80000000 | (vsid << 7) | 0x40 | ((addr & 0xfc00000)>>22)) == pte_h) break;
	}

	if(i != 8) //secondary hit
	{
		pte_l |= 0x180; //set rc
//		WARN_LOG(MASTER_LOG, "setting RC bits in %08x because of %08x[S]", pteg_addr+i*8, addr);
		write32_ne(EmuPointer(pteg_addr+i*8+4), pte_l, ACCESS_MASK_PHYSICAL);
		return write_sz_ne(EmuPointer(addr), in);
	}

	// problem! couldn't find the matching pte
	// do something smarter here
	WARN_LOG(MASTER_LOG, "COULD NOT FIND PTE!!!!: %08x", addr);

	return write_sz_ne(EmuPointer(addr), in);
}


static inline void map_ipages(int a_mask, u32 virt_page_start, u32 page_count, u32 phys_page_start, u32 pp, u32 flags=MAP_TYPE_BAT)
{
//	WARN_LOG(MASTER_LOG, "map_ipages(0x%02hhx, 0x%08x, 0x%08x, 0x%08x, 0x%08x)", a_mask, virt_page_start, page_count, phys_page_start, pp);
	u32 new_type = flags & MAP_TYPE_MASK;
	for(u32 i=0; i<page_count; i++)
	{
		u32 old_type = memory_access[a_mask][virt_page_start + i].typei & MAP_TYPE_MASK;
		if((new_type == MAP_TYPE_PTE) && (old_type == MAP_TYPE_BAT))
		{
			WARN_LOG(MASTER_LOG, "map_ipages(0x%02hhx, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0%08x) TRIED TO MAP PTE TO BAT REGION!", a_mask, virt_page_start, page_count, phys_page_start, pp, flags);
			continue;
		}
		memory_access[a_mask][virt_page_start + i].typei = new_type;
		switch(flags&3)
		{
			case MAP_PAGE_NO_RC:
			{
				memory_access[a_mask][virt_page_start + i].iaf.read_u32_exec = &read_via_pt_no_r<u32>;
				memory_access[a_mask][virt_page_start + i].contexti = (void*)1;
			}
			default:
			{
				memory_access[a_mask][virt_page_start + i].iaf = memory_access[ACCESS_MASK_PHYSICAL][phys_page_start + i].iaf;
				memory_access[a_mask][virt_page_start + i].contexti = memory_access[ACCESS_MASK_PHYSICAL][phys_page_start + i].contexti;
				if(PP_NOACCESS(pp))
				{
					memory_access[a_mask][virt_page_start + i].iaf.read_u32_exec = &trigger_read_exception<u32, ISI_PROT>;
				}
				break;
			}
		}
	}
}

static inline void unmap_ipages(int a_mask, u32 virt_page_start, u32 page_count, u32 flags=0)
{
//	WARN_LOG(MASTER_LOG, "unmap_ipages(0x%02hhx, 0x%08x, 0x%08x)", a_mask, virt_page_start, page_count);
	for(u32 i=0; i<page_count; i++)
	{
		if((memory_access[a_mask][virt_page_start + i].typei & flags & MAP_TYPE_MASK)==0) continue;
		memory_access[a_mask][virt_page_start + i].iaf = INoTranslation;
		memory_access[a_mask][virt_page_start + i].contexti = NULL;
		memory_access[a_mask][virt_page_start + i].typei = 0;
	}
}

static inline void map_dpages(int a_mask, u32 virt_page_start, u32 page_count, u32 phys_page_start, u32 pp, u32 flags=MAP_TYPE_BAT)
{
//	WARN_LOG(MASTER_LOG, "map_dpages(0x%02hhx, 0x%08x, 0x%08x, 0x%08x, 0x%08x)", a_mask, virt_page_start, page_count, phys_page_start, pp);
	u32 new_type = flags & MAP_TYPE_MASK;
	for(u32 i=0; i<page_count; i++)
	{
		u32 old_type = memory_access[a_mask][virt_page_start + i].typed & MAP_TYPE_MASK;
		if((new_type == MAP_TYPE_PTE) && (old_type == MAP_TYPE_BAT)) continue;
		memory_access[a_mask][virt_page_start + i].typed = new_type;
		memory_access[a_mask][virt_page_start + i].daf = memory_access[ACCESS_MASK_PHYSICAL][phys_page_start + i].daf;
		memory_access[a_mask][virt_page_start + i].contextd = memory_access[ACCESS_MASK_PHYSICAL][phys_page_start + i].contextd;
		switch(flags&3)
		{
			case MAP_PAGE_NO_RC:
			{
				memory_access[a_mask][virt_page_start + i].daf.read_u8 = &read_via_pt_no_r<u8>;
				memory_access[a_mask][virt_page_start + i].daf.read_u16 = &read_via_pt_no_r<u16>;
				memory_access[a_mask][virt_page_start + i].daf.read_u32 = &read_via_pt_no_r<u32>;
				memory_access[a_mask][virt_page_start + i].daf.read_u64 = &read_via_pt_no_r<u64>;
			}
			// intentional fallthrough
			case MAP_PAGE_NO_C:
			{
				memory_access[a_mask][virt_page_start + i].daf.write_u8 = &write_via_pt_no_c<u8>;
				memory_access[a_mask][virt_page_start + i].daf.write_u16 = &write_via_pt_no_c<u16>;
				memory_access[a_mask][virt_page_start + i].daf.write_u32 = &write_via_pt_no_c<u32>;
				memory_access[a_mask][virt_page_start + i].daf.write_u64 = &write_via_pt_no_c<u64>;
			}
		}

		if(!PP_RW(pp))
		{
			memory_access[a_mask][virt_page_start + i].daf.write_u8 = &trigger_write_exception<u8, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.write_u16 = &trigger_write_exception<u16, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.write_u32 = &trigger_write_exception<u32, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.write_u64 = &trigger_write_exception<u64, DSI_PROT>;
		}
		if(PP_NOACCESS(pp))
		{
			memory_access[a_mask][virt_page_start + i].daf.read_u8 = &trigger_read_exception<u8, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.read_u16 = &trigger_read_exception<u16, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.read_u32 = &trigger_read_exception<u32, DSI_PROT>;
			memory_access[a_mask][virt_page_start + i].daf.read_u64 = &trigger_read_exception<u64, DSI_PROT>;
		}

	}
}
static inline void unmap_dpages(int a_mask, u32 virt_page_start, u32 page_count, u32 flags=0)
{
//	WARN_LOG(MASTER_LOG, "unmap_dpages(0x%02hhx, 0x%08x, 0x%08x)", a_mask, virt_page_start, page_count);
	for(u32 i=0; i<page_count; i++)
	{
		if((memory_access[a_mask][virt_page_start + i].typed & flags & MAP_TYPE_MASK)==0) continue;
		memory_access[a_mask][virt_page_start + i].daf = DNoTranslation;
		memory_access[a_mask][virt_page_start + i].contextd = NULL;
		memory_access[a_mask][virt_page_start + i].typed = 0;
	}
}


template <typename ACCESS_TYPE>
int normal_read(const void* context, const u32 addr, ACCESS_TYPE &out)
{
	const u8 *real_base = (u8*)context;
	out = swapper<ACCESS_TYPE>(*(ACCESS_TYPE*)(real_base+(addr & PAGE_MASK)));
	return 0;
}

template <typename ACCESS_TYPE>
int normal_write(void* context, const u32 addr, const ACCESS_TYPE in)
{
	u8 *real_base = (u8*)context;
	*(ACCESS_TYPE*)(real_base+(addr & PAGE_MASK)) = swapper<ACCESS_TYPE>(in);
	return 0;
}

template <typename ACCESS_TYPE>
int no_exception_error_read(const void* context, const u32 addr, ACCESS_TYPE &out)
{
#ifdef MMU_ON_ERROR_READ_WARN
	WARN_LOG(MASTER_LOG, "error_read: trying to read%lu(0x%08x)", sizeof(ACCESS_TYPE)*8, addr);
#endif
	return -1;
}

template <typename ACCESS_TYPE>
int no_exception_error_write(void* context, const u32 addr, const ACCESS_TYPE in)
{
#ifdef MMU_ON_ERROR_WRITE_WARN
	WARN_LOG(MASTER_LOG, "error_write: trying to write%lu(0x%08x)", sizeof(ACCESS_TYPE)*8, addr);
#endif
	return -1;
}

static void unmap_pte(int pteg_index, u32 pteh, u32 ptel)
{
	if((pteh & 0x80000000)==0) return;
	u32 vsid = (pteh >> 7)&0xffffff;
	int i;
	for(i=0;i<16; i++)
	{
		if((PowerPC::ppcState.sr[i] & 0xffffff)==vsid) break;
	}
	if(i==16)
	{
//		WARN_LOG(MASTER_LOG, "couldn't find vsid in segment registers, sad day");
		for(i=0;i<4; i++)
		{
			WARN_LOG(MASTER_LOG, "SR%02d: 0x%08x SR%02d: 0x%08x SR%02d: 0x%08x SR%02d: 0x%08x",
				4*i, PowerPC::ppcState.sr[4*i],
				4*i+1, PowerPC::ppcState.sr[4*i+1],
				4*i+2, PowerPC::ppcState.sr[4*i+2],
				4*i+3, PowerPC::ppcState.sr[4*i+3]);
		}
		return;
	}
	

	u32 morebits = (vsid ^ pteg_index);
	if(pteh & 0x40) morebits = ~morebits;
	morebits &= 0x3ff;

	u32 api = pteh & 0x3f;
	
	u32 target_va_page_addr = (i<<28)| (api<<22) | (morebits)<<12;
//	u32 sr_options = PowerPC::ppcState.sr[i];


#ifdef MMU_ON_UNMAP_PTE_WARN
	if(ptel != 0)
	{
		WARN_LOG(MASTER_LOG, "page_table: unmapping 0x%08x [pteh: %08x ptel: %08x sr: %08x] ", target_va_page_addr, pteh, ptel, PowerPC::ppcState.sr[i]);
	}
#endif


	
	

	for(int j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_ON)
		{
			unmap_ipages(j, target_va_page_addr>>12, 1, MAP_TYPE_PTE);
		}
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_DR_ON)
		{
			unmap_dpages(j, target_va_page_addr>>12, 1, MAP_TYPE_PTE);
		}
	}
}


void map_pte(int pteg_index, u32 pteh, u32 ptel)
{
	if((pteh & 0x80000000)==0) return;

//	WARN_LOG(MASTER_LOG, "void map_pte(%d, 0x%08x, 0x%08x)", pteg_index, pteh, ptel);
	u32 vsid = (pteh >> 7)&0xffffff;
	int i;
	for(i=0;i<16; i++)
	{
		if((PowerPC::ppcState.sr[i] & 0xffffff)==vsid) break;
	}
	if(i==16)
	{
		// vsid not set yey?
		// ok, but maybe we can do something later..
/*
		WARN_LOG(MASTER_LOG, "couldn't find vsid in segment registers, sad day");
		for(i=0;i<4; i++)
		{
			WARN_LOG(MASTER_LOG, "SR%02d: 0x%08x SR%02d: 0x%08x SR%02d: 0x%08x SR%02d: 0x%08x",
				4*i, PowerPC::ppcState.sr[4*i],
				4*i+1, PowerPC::ppcState.sr[4*i+1],
				4*i+2, PowerPC::ppcState.sr[4*i+2],
				4*i+3, PowerPC::ppcState.sr[4*i+3]);
		}
*/
		return;
	}

	u32 morebits = (vsid ^ pteg_index);
	if(pteh & 0x40) morebits = ~morebits;
	morebits &= 0x3ff;

	u32 api = pteh & 0x3f;
	
	u32 target_va_page_addr = (i<<28)| (api<<22) | (morebits)<<12;
	u32 target_phys_page_addr = ptel & 0xfffff000;


#ifdef MMU_ON_MAP_PTE_WARN
	if(ptel != 0)
	{
		WARN_LOG(MASTER_LOG, "page_table: mapping 0x%08x => 0x%08x [pteh: %08x ptel: %08x sr: %08x] ", target_phys_page_addr, target_va_page_addr, pteh, ptel, PowerPC::ppcState.sr[i]);
	}
#endif

	if(target_va_page_addr == 0x80163000)
	{
		api=api;
	}
	u32 p_pp = ptel & 0x00000003;
	u32 sr_options = PowerPC::ppcState.sr[i];

	int flags = MAP_PAGE_NORMAL;
	switch(ptel & 0x180)
	{
		case 0x000:
		case 0x080: // this should never happen, but let's just map it to the no_r no_c case
		{
			flags = MAP_PAGE_NO_RC;
			break;
		}
		case 0x100:
		{
			flags = MAP_PAGE_NO_C;
			break;
		}
	}

	for(int j=1; j<ACCESS_COUNT; j++)
	{
		u32 key = (
				((j & ACCESS_MASK_PR) &&
					(sr_options & SR_OPTION_Kp)) ||
				(!(j & ACCESS_MASK_PR) && 
					(sr_options & SR_OPTION_Ks))
				)?1:0;

		u32 pp = 0;
		switch((key<<2)|p_pp)
		{
			case 0:
			case 1:
			case 2:
			case 6:
			{
				pp=2;
				break;
			}
			case 3:
			case 5:
			case 7:
			{
				pp=1;
				break;
			}
			default:
			{
				pp=0;
				break;
			}
		}
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_ON)
		{
			if((sr_options & SR_OPTION_N)==0)
			{
				map_ipages(j, target_va_page_addr>>12, 1, target_phys_page_addr>>12, pp, flags | MAP_TYPE_PTE);
			}
		}
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_DR_ON)
		{
			map_dpages(j, target_va_page_addr>>12, 1, target_phys_page_addr>>12, pp, flags | MAP_TYPE_PTE);
		}
	}
}


/*
not complete


*/

template <typename ACCESS_TYPE>
int pagetable_write(void* context, const u32 addr, const ACCESS_TYPE in)
{
	int rv;
#ifdef MMU_ON_PAGETABLE_WRITE_WARNING
	WARN_LOG(MASTER_LOG, "Program pagetable_write(%08x, 0x%llx, %lu) @ %08x", addr, (u64)in, sizeof(ACCESS_TYPE)*8, PowerPC::ppcState.pc);
#endif
	u8 *real_base = (u8*)context;
	u32 pte_base_addr = addr & ~7;

	u32 pt_bits = ((PowerPC::ppcState.spr[SPR_SDR] & 0x1ff)<<16) | 0xffc0;
	
	// unmap whatever this pte was mapping
	u32 pte_h = swapper<u32>(*(u32*)(real_base+(pte_base_addr & PAGE_MASK)));
	u32 pte_l = swapper<u32>(*(u32*)(real_base+((pte_base_addr+4) & PAGE_MASK)));
	unmap_pte((pte_base_addr&pt_bits) >> 6, pte_h, pte_l);

	
	rv = normal_write<ACCESS_TYPE>(context, addr, in);

	// map whatever this pte is now mapping
	pte_h = swapper<u32>(*(u32*)(real_base+(pte_base_addr & PAGE_MASK)));
	pte_l = swapper<u32>(*(u32*)(real_base+((pte_base_addr+4) & PAGE_MASK)));
	map_pte((pte_base_addr&pt_bits) >> 6, pte_h, pte_l);

	return rv;
}



static struct DAccessFuncs DPageTableFuncs =
{
	&normal_read<u8>,
	&normal_read<u16>,
	&normal_read<u32>,
	&normal_read<u64>,
	&pagetable_write<u8>,
	&pagetable_write<u16>,
	&pagetable_write<u32>,
	&pagetable_write<u64>,
};


static struct DAccessFuncs DRWAccess =
{
	&normal_read<u8>,
	&normal_read<u16>,
	&normal_read<u32>,
	&normal_read<u64>,
	&normal_write<u8>,
	&normal_write<u16>,
	&normal_write<u32>,
	&normal_write<u64>,
};

int normal_read32_instr(const void* context, const u32 addr, u32 &out)
{
	const u8 *real_base = reinterpret_cast<const u8*>(context);
	out = Common::swap32(*(u32*)(real_base+(addr&PAGE_MASK)));
	return 0;
}

static struct IAccessFuncs IAccess =
{
	&normal_read32_instr,
};

u32 access_mask = 0;

void set_access_mask(u32 new_access_mask)
{
	if(new_access_mask < ACCESS_COUNT) access_mask = new_access_mask;
}

void on_msr_change()
{
	resync_access_mask();
#ifdef MMU_ON_MSR_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_msr_change() access_mask=0x%08x", access_mask);
#endif
}


void reset()
{
	_dbg_assert_msg_(MEMMAP, (PAGE_SIZE & PAGE_MASK)==0, "PAGE_SIZE(%d) and PAGE_MASK(%d) are bad values", PAGE_SIZE, PAGE_MASK);
	for(int j=0; j<ACCESS_COUNT; j++)
	{
		for(int i=0; i<0x100000; i++)
		{
			memory_access[j][i].daf = DNoTranslation;
			memory_access[j][i].iaf = INoTranslation;
			memory_access[j][i].contextd = NULL;
			memory_access[j][i].contexti = NULL;
		}
	}
}

void reset_mappings()
{
	WARN_LOG(MASTER_LOG, "********* RESET MMU MAPPINGS *********");
	
	for(int j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_DR_ON)
		{
			for(int i=0; i<0x100000; i++)
			{
				memory_access[j][i].daf = DNoTranslation;
				memory_access[j][i].contextd = NULL;
			}
		}
		else
		{
			for(int i=0; i<0x100000; i++)
			{
				if(memory_access[j][i].daf == DPageTableFuncs)
				{
					memory_access[j][i].daf = DRWAccess;
				}
			}
		}
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_ON)
		{
			for(int i=0; i<0x100000; i++)
			{
				memory_access[j][i].iaf = INoTranslation;
				memory_access[j][i].contexti = NULL;
			}
		}
	}
}

void daf_reset_to_no_except(DAccessFuncs *daf)
{
	daf->read_u8 = &no_exception_error_read<u8>;
	daf->read_u16 = &no_exception_error_read<u16>;
	daf->read_u32 = &no_exception_error_read<u32>;
	daf->read_u64 = &no_exception_error_read<u64>;
	daf->write_u8 = &no_exception_error_write<u8>;
	daf->write_u16 = &no_exception_error_write<u16>;
	daf->write_u32 = &no_exception_error_write<u32>;
	daf->write_u64 = &no_exception_error_write<u64>;
}

void map_mmio_device(DAccessFuncs *daf, void *contextd, u32 phys_size, u32 phys_addr)
{
	u32 count = phys_size >> 12;
	u32 page = phys_addr >> 12;
	for(u32 j=0; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_DR_OFF)
		{
			for(u32 i=0; i<count; i++)
			{
				memory_access[j][page + i].daf = *daf;
				memory_access[j][page + i].contextd = contextd;
			}
		}
	}

}

void map_physical(u8 *real_base, u32 phys_size, u32 phys_addr)
{
//	assert((phys_size%PAGE_SIZE)==0);
	u32 count = phys_size >> 12;
	u32 page = phys_addr >> 12;
	for(u32 j=0; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_DR_OFF)
		{
			for(u32 i=0; i<count; i++)
			{
				memory_access[j][page + i].daf = DRWAccess;
				memory_access[j][page + i].contextd = real_base + i*PAGE_SIZE;
			}
		}
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_OFF)
		{
			for(u32 i=0; i<count; i++)
			{
				memory_access[j][page + i].iaf = IAccess;
				memory_access[j][page + i].contexti = real_base + i*PAGE_SIZE;
			}
		}
	}
}

const u32 BATU_BEPI_MASK=0xFFFE0000;
const u32 BATU_BL_MASK=0x00001FFC; 
const u32 BATU_SP_MASK=0x00000003;
const u32 BATU_Vs=0x00000002;
const u32 BATU_Vp=0x00000001;
const u32 BATL_BRPN_MASK=0xFFFE0000;
const u32 BATL_PP_MASK=0x00000003;



void on_ibatl_change(int index, u32 ibatu_value, u32 ibatl_value)
{
#ifdef MMU_ON_IBAT_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_ibatl_change(%d, %08x, %08x)", index, ibatu_value, ibatl_value);
#endif
	u32 base_virt_page = (ibatu_value & BATU_BEPI_MASK)>>12;
	u32 count = ((ibatu_value & BATU_BL_MASK)+4) << 3;
	u32 sp = (ibatu_value & BATU_SP_MASK);
	u32 brpn_phys_page = (ibatl_value & BATL_BRPN_MASK)>>12;

	u32 pp = ibatl_value & BATL_PP_MASK;


#ifdef MMU_ON_MAP_BAT_WARN
	if(ibatu_value != 0)
	{
		WARN_LOG(MASTER_LOG, "IBAT%d: %08x -> %08x [size: %08x]", index, brpn_phys_page<<12, base_virt_page<<12, count<<12);
	}
#endif

	for(u32 j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_OFF) continue;

		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (sp & BATU_Vs))
		)
		{
			map_ipages(j, base_virt_page, count, brpn_phys_page, pp, MAP_TYPE_BAT);
		}
	}
}

void on_ibatu_change(int index, u32 ibatu_old_value, u32 ibatu_new_value, u32 ibatl_value)
{
#ifdef MMU_ON_IBAT_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_ibatu_change(%d, %08x, %08x, %08x)", index, ibatu_old_value, ibatu_new_value, ibatl_value);
#endif
	u32 old_base_virt_page = (ibatu_old_value & BATU_BEPI_MASK)>>12;
	u32 old_count = ((ibatu_old_value & BATU_BL_MASK)+4) << 3;
	u32 old_sp = (ibatu_old_value & BATU_SP_MASK);
	
	u32 new_base_virt_page = (ibatu_new_value & BATU_BEPI_MASK)>>12;
	u32 new_count = ((ibatu_new_value & BATU_BL_MASK)+4) << 3;
	u32 new_sp = (ibatu_new_value & BATU_SP_MASK);

	u32 brpn_phys_page = (ibatl_value & BATL_BRPN_MASK)>>12;
	u32 pp = ibatl_value & BATL_PP_MASK;

	

#ifdef MMU_ON_MAP_BAT_WARN
	if(ibatu_new_value != 0)
	{
		WARN_LOG(MASTER_LOG, "IBAT%d: %08x -> %08x [size: %08x]", index, brpn_phys_page<<12, new_base_virt_page<<12, new_count<<12);
	}
#endif

	for(u32 j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_OFF) continue;
		
		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (old_sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (old_sp & BATU_Vs))
		)
		{
			unmap_ipages(j, old_base_virt_page, old_count, MAP_TYPE_BAT);
		}
		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (new_sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (new_sp & BATU_Vs))
		)
		{
			map_ipages(j, new_base_virt_page, new_count, brpn_phys_page, pp, MAP_TYPE_BAT);
		}
	}
}

void on_dbatl_change(int index, u32 dbatu_value, u32 dbatl_value)
{
#ifdef MMU_ON_DBAT_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_dbatl_change(%d, %08x, %08x)", index, dbatu_value, dbatl_value);
#endif
	u32 base_virt_page = (dbatu_value & BATU_BEPI_MASK)>>12;
	u32 count = ((dbatu_value & BATU_BL_MASK)+4) << 3;
	u32 sp = (dbatu_value & BATU_SP_MASK);
	u32 brpn_phys_page = (dbatl_value & BATL_BRPN_MASK)>>12;

	u32 pp = dbatl_value & BATL_PP_MASK;


#ifdef MMU_ON_MAP_BAT_WARN
	if(dbatu_value != 0)
	{
		WARN_LOG(MASTER_LOG, "DBAT%d: %08x -> %08x [size: %08x]", index, brpn_phys_page<<12, base_virt_page<<12, count<<12);
	}
#endif


	for(u32 j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_DR) == ACCESS_MASK_IR_OFF) continue;

		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (sp & BATU_Vs))
		)
		{
			map_dpages(j, base_virt_page, count, brpn_phys_page, pp);
		}
	}
}

void on_dbatu_change(int index, u32 dbatu_old_value, u32 dbatu_new_value, u32 dbatl_value)
{
#ifdef MMU_ON_DBAT_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_dbatu_change(%d, %08x, %08x, %08x)", index, dbatu_old_value, dbatu_new_value, dbatl_value);
#endif
	u32 old_base_virt_page = (dbatu_old_value & BATU_BEPI_MASK)>>12;
	u32 old_count = ((dbatu_old_value & BATU_BL_MASK)+4) << 3;
	u32 old_sp = (dbatu_old_value & BATU_SP_MASK);
	
	u32 new_base_virt_page = (dbatu_new_value & BATU_BEPI_MASK)>>12;
	u32 new_count = ((dbatu_new_value & BATU_BL_MASK)+4) << 3;
	u32 new_sp = (dbatu_new_value & BATU_SP_MASK);

	u32 brpn_phys_page = (dbatl_value & BATL_BRPN_MASK)>>12;
	u32 pp = dbatl_value & BATL_PP_MASK;


#ifdef MMU_ON_MAP_BAT_WARN
	if(dbatu_new_value != 0)
	{
		WARN_LOG(MASTER_LOG, "DBAT%d: %08x -> %08x [size: %08x]", index, brpn_phys_page<<12, new_base_virt_page<<12, new_count<<12);
	}
#endif

	for(u32 j=1; j<ACCESS_COUNT; j++)
	{
		if((j & ACCESS_MASK_IR) == ACCESS_MASK_IR_OFF) continue;
		
		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (old_sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (old_sp & BATU_Vs))
		)
		{
			unmap_dpages(j, old_base_virt_page, old_count);
		}
		if(
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_USER) && (new_sp & BATU_Vp)) ||
			(((j & ACCESS_MASK_PR) == ACCESS_MASK_PR_SUPER) && (new_sp & BATU_Vs))
		)
		{
			map_dpages(j, new_base_virt_page, new_count, brpn_phys_page, pp);
		}
	}
}


static inline void bat_sync()
{
	on_dbatl_change(0, PowerPC::ppcState.spr[SPR_DBAT0U], PowerPC::ppcState.spr[SPR_DBAT0L]);
	on_dbatl_change(1, PowerPC::ppcState.spr[SPR_DBAT1U], PowerPC::ppcState.spr[SPR_DBAT1L]);
	on_dbatl_change(2, PowerPC::ppcState.spr[SPR_DBAT2U], PowerPC::ppcState.spr[SPR_DBAT2L]);
	on_dbatl_change(3, PowerPC::ppcState.spr[SPR_DBAT3U], PowerPC::ppcState.spr[SPR_DBAT3L]);
	on_ibatl_change(0, PowerPC::ppcState.spr[SPR_IBAT0U], PowerPC::ppcState.spr[SPR_IBAT0L]);
	on_ibatl_change(1, PowerPC::ppcState.spr[SPR_IBAT1U], PowerPC::ppcState.spr[SPR_IBAT1L]);
	on_ibatl_change(2, PowerPC::ppcState.spr[SPR_IBAT2U], PowerPC::ppcState.spr[SPR_IBAT2L]);
	on_ibatl_change(3, PowerPC::ppcState.spr[SPR_IBAT3U], PowerPC::ppcState.spr[SPR_IBAT3L]);
}


/*
void destroy_pagetable(u32 sdr)
{
	u32 base_page_index = (sdr>>16)<<4;
	u32 page_count = ((sdr&0x1ff)+1)<<4;
	// parse page table and adjust relevant page access funcs

	u32 pt_bits = ((PowerPC::ppcState.spr[SPR_SDR] & 0x1ff)<<16) | 0xffff;
	u32 pt_base = PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000;

	set_access_mask(ACCESS_MASK_PHYSICAL);
	u32 pteh, ptel;
	for(u32 i=0; i<(pt_bits+1); i+=8)
	{
		int k = read32(EmuPointer(pt_base+i), &pteh) + mmu_read32(EmuPointer(pt_base+i+4), &ptel);
		if(k==2)
		{
			unmap_pte(i>>6, pteh, ptel);
		}
	}
	resync_access_mask();


	// reset page table access funcs
	for(u32 i=0; i<page_count; i++)
	{
		if(memory_access[ACCESS_MASK_PHYSICAL][base_page_index + i].contextd)
		{
			memory_access[ACCESS_MASK_PHYSICAL][base_page_index + i].daf = DRWAccess;
		}
	}
	// run bat restore
	bat_restore();
}
*/

static inline void construct_pagetable(u32 sdr)
{
	if(sdr == 0) return;
	// setup page table access funcs
	u32 base_page_index = (sdr>>16)<<4;
	u32 page_count = ((sdr&0x1ff)+1)<<4;
	u32 pt_bits = ((PowerPC::ppcState.spr[SPR_SDR] & 0x1ff)<<16) | 0xffff;
	u32 pt_base = PowerPC::ppcState.spr[SPR_SDR] & 0xffff0000;
	for(u32 i=0; i<page_count; i++)
	{
		if(memory_access[ACCESS_MASK_PHYSICAL][base_page_index + i].contextd)
		{
			memory_access[ACCESS_MASK_PHYSICAL][base_page_index + i].daf = DPageTableFuncs;
		}
	}
	// parse page table and adjust relevant page access funcs
	u32 pteh, ptel;
	for(u32 i=0; i<(pt_bits+1); i+=8)
	{
		int k = read32(EmuPointer(pt_base+i), pteh, ACCESS_MASK_PHYSICAL) | read32(EmuPointer(pt_base+i+4), ptel, ACCESS_MASK_PHYSICAL);
		if(k==0)
		{
			map_pte(i>>6, pteh, ptel);
		}
	}
	// bat restore happens in caller
	
}

void on_sdr_change(u32 new_value)
{
#ifdef MMU_ON_SDR_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_sdr_change() new_value=0x%08x",  new_value);
#endif

	reset_mappings();
	construct_pagetable(PowerPC::ppcState.spr[SPR_SDR]);
	bat_sync();

}

void on_sr_change(int sr, u32 new_value)
{
#ifdef MMU_ON_SR_CHANGE_WARNING
	WARN_LOG(MASTER_LOG, "Program on_sr_change() sr=0x%02hhx new_value=0x%08x", sr, new_value);
#endif
//	reset_mappings();
//	construct_pagetable(PowerPC::ppcState.spr[SPR_SDR]);
//	bat_sync();
}

u8 *get_phys_addr_ptr(const EmuPointer &addr)
{
	u8 *base = reinterpret_cast<u8*>(memory_access[ACCESS_MASK_PHYSICAL][addr.m_addr>>12].contextd);
	if(base == NULL) return NULL;
	return &base[addr.m_addr&PAGE_MASK];
}

/*


EmuPointer memcpy_real_to_emu(EmuPointer dst, const void *src, size_t n)
{
	EmuPointer rv(dst.m_addr);
	const u8 *u8_src = (const u8*)src;
	if(n && (dst.m_addr & 1))
	{
		raw_write8(dst.m_addr, *u8_src);
		n-=sizeof(u8);
		dst.m_addr+=sizeof(u8);
		u8_src+=sizeof(u8);
		
	}
	if(n && (dst.m_addr & 2))
	{
		raw_write16(dst.m_addr, *(const u16*)u8_src);
		n-=sizeof(u16);
		dst.m_addr+=sizeof(u16);
		u8_src+=sizeof(u16);
	}
	if(n && (dst.m_addr & 4))
	{
		raw_write32(dst.m_addr, *(const u32*)u8_src);
		n-=sizeof(u32);
		dst.m_addr+=sizeof(u32);
		u8_src+=sizeof(u32);
		
	}
	while(n>=sizeof(u64))
	{
		raw_write64(dst.m_addr, *(const u64*)u8_src);
		n-=sizeof(u64);
		dst.m_addr+=sizeof(u64);
		u8_src+=sizeof(u64);
	}
	if(n&4)
	{
		raw_write32(dst.m_addr, *(const u32*)u8_src);
		dst.m_addr+=sizeof(u32);
		u8_src+=sizeof(u32);
	}
	if(n&2)
	{
		raw_write16(dst.m_addr, *(const u16*)u8_src);
		dst.m_addr+=sizeof(u16);
		u8_src+=sizeof(u16);
	}
	if(n&1)
	{
		raw_write8(dst.m_addr, *u8_src);
	}
	return rv;
}

void *memcpy_emu_to_real(void *dst, EmuPointer src, size_t n)
{
	void *rv = dst;
	u8* u8_dst = (u8*)dst;
	if(n && (src.m_addr & 1))
	{
		raw_read8(src.m_addr, u8_dst);
		n-=sizeof(u8);
		src.m_addr+=sizeof(u8);
		u8_dst+=sizeof(u8);
		
	}
	if(n && (src.m_addr & 2))
	{
		raw_read16(src.m_addr, (u16*)u8_dst);
		n-=sizeof(u16);
		src.m_addr+=sizeof(u16);
		u8_dst+=sizeof(u16);
	}
	if(n && (src.m_addr & 4))
	{
		raw_read32(src.m_addr, (u32*)u8_dst);
		n-=sizeof(u32);
		src.m_addr+=sizeof(u32);
		u8_dst+=sizeof(u32);
		
	}
	while(n>=sizeof(u64))
	{
		raw_read64(src.m_addr, (u64*)u8_dst);
		n-=sizeof(u64);
		src.m_addr+=sizeof(u64);
		u8_dst+=sizeof(u64);
	}
	if(n&4)
	{
		raw_read32(src.m_addr, (u32*)u8_dst);
		src.m_addr+=sizeof(u32);
		u8_dst+=sizeof(u32);
	}
	if(n&2)
	{
		raw_read16(src.m_addr, (u16*)u8_dst);
		src.m_addr+=sizeof(u16);
		u8_dst+=sizeof(u16);
	}
	if(n&1)
	{
		raw_read8(src.m_addr, u8_dst);
	}
	return rv;
}
*/

EmuPointer memset_emu(EmuPointer s, int c, size_t n)
{
	u8 buffer[sizeof(u64)];
	memset(buffer, c, sizeof(u64));

	EmuPointer rv(s.m_addr);
	if(n && (s.m_addr & 1))
	{
		write8(s, buffer[0]);
		n-=sizeof(u8);
		s.m_addr+=sizeof(u8);
	}
	if(n && (s.m_addr & 2))
	{
		write16(s, *(const u16*)buffer);
		n-=sizeof(u16);
		s.m_addr+=sizeof(u16);
	}
	if(n && (s.m_addr & 4))
	{
		write32(s, *(const u32*)buffer);
		n-=sizeof(u32);
		s.m_addr+=sizeof(u32);
		
	}
	while(n>=sizeof(u64))
	{
		write64(s, *(const u64*)buffer);
		n-=sizeof(u64);
		s.m_addr+=sizeof(u64);
	}
	if(n&4)
	{
		write32(s, *(const u32*)buffer);
		s.m_addr+=sizeof(u32);
	}
	if(n&2)
	{
		write16(s, *(const u16*)buffer);
		s.m_addr+=sizeof(u16);
	}
	if(n&1)
	{
		write8(s, *buffer);
	}
	return rv;
	
}

} //namespace MMUTable
