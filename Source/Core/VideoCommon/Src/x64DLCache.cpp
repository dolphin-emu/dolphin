// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO: Handle cache-is-full condition :p

#include <map>

#include "Common.h"
#include "VideoCommon.h"
#include "Hash.h"
#include "MemoryUtil.h"
#include "DataReader.h"
#include "Statistics.h"
#include "OpcodeDecoding.h"   // For the GX_ constants.
#include "HW/Memmap.h"

#include "XFMemory.h"
#include "CPMemory.h"
#include "BPMemory.h"

#include "VertexLoaderManager.h"
#include "VertexManagerBase.h"
#include "x64Emitter.h"
#include "x64ABI.h"

#include "DLCache.h"
#include "VideoConfig.h"

#define DL_CODE_CACHE_SIZE (1024*1024*16)
#define DL_CODE_CLEAR_THRESHOLD (16 * 1024)
extern int frameCount;
static u32 CheckContextId;
using namespace Gen;

namespace DLCache
{

enum DisplayListPass {
	DLPASS_ANALYZE,
	DLPASS_COMPILE,
	DLPASS_RUN,
};

#define DL_HASH_STEPS 512

struct ReferencedDataRegion
{
	ReferencedDataRegion()
		:hash(0),
		start_address(NULL),
		NextRegion(NULL),
		size(0),
		MustClean(0)
	{}
	u64 hash;
	u8* start_address;
	ReferencedDataRegion* NextRegion;
	u32 size;
	u32 MustClean;
	

	int IntersectsMemoryRange(u8* range_address, u32 range_size)
	{
		if (start_address + size < range_address)
			return -1;
		if (start_address >= range_address + range_size)
			return 1;
		return 0;
	}
};

struct CachedDisplayList
{
	CachedDisplayList()
			: Regions(NULL),
			LastRegion(NULL),
			uncachable(false),
			num_xf_reg(0),
			num_cp_reg(0),
			num_bp_reg(0), 
			num_index_xf(0),
			num_draw_call(0),
			pass(DLPASS_ANALYZE),
			BufferCount(0)
	{
		frame_count = frameCount;
	}
	u64 dl_hash;
	// ... Something containing cached vertex buffers here ...
	ReferencedDataRegion* Regions;
	ReferencedDataRegion* LastRegion;
	// Compile the commands themselves down to native code.
	const u8* compiled_code;
	u32 uncachable;  // if set, this DL will always be interpreted. This gets set if hash ever changes.
	// Analytic data
	u32 num_xf_reg;
	u32 num_cp_reg;
	u32 num_bp_reg; 
	u32 num_index_xf;
	u32 num_draw_call;
	u32 pass;
	u32 check;
	int frame_count;
	u32 BufferCount;

	void InsertRegion(ReferencedDataRegion* NewRegion)
	{
		if(LastRegion)
		{
			LastRegion->NextRegion = NewRegion;
		}
		LastRegion = NewRegion;
		if(!Regions)
		{
			Regions = LastRegion;
		}
		BufferCount++;
	}

	void InsertOverlapingRegion(u8* RegionStartAddress, u32 Size)
	{
		ReferencedDataRegion* NewRegion = FindOverlapingRegion(RegionStartAddress, Size);
		if(NewRegion)
		{
			bool RegionChanged = false;
			if(RegionStartAddress < NewRegion->start_address)
			{
				NewRegion->start_address = RegionStartAddress;
				RegionChanged = true;
			}
			if(RegionStartAddress + Size > NewRegion->start_address + NewRegion->size)
			{
				NewRegion->size += (u32)((RegionStartAddress + Size) - (NewRegion->start_address + NewRegion->size));
				RegionChanged = true;
			}
			if(RegionChanged)
				NewRegion->hash = GetHash64(NewRegion->start_address, NewRegion->size, DL_HASH_STEPS);
		}
		else
		{
			NewRegion = new ReferencedDataRegion;
			NewRegion->MustClean = false;
			NewRegion->size = Size;
			NewRegion->start_address = RegionStartAddress; 
			NewRegion->hash = GetHash64(RegionStartAddress, Size, DL_HASH_STEPS);
			InsertRegion(NewRegion);
		}
	}

	bool CheckRegions()
	{
		ReferencedDataRegion* Current = Regions;
		while(Current)
		{
			if(Current->hash)
			{
				if(Current->hash != GetHash64(Current->start_address,  Current->size, DL_HASH_STEPS))
					return false;
			}
			Current = Current->NextRegion;
		}
		return true;
	}

	ReferencedDataRegion* FindOverlapingRegion(u8* RegionStart, u32 Regionsize)
	{
		ReferencedDataRegion* Current = Regions;
		while(Current)
		{
			if(!Current->IntersectsMemoryRange(RegionStart, Regionsize))
				return Current;
			Current = Current->NextRegion;
		}
		return Current;
	}

	void ClearRegions()
	{
		ReferencedDataRegion* Current = Regions;
		while(Current)
		{
			ReferencedDataRegion* temp = Current;
			Current = Current->NextRegion;
			if(temp->MustClean)
				delete [] temp->start_address;
			delete temp;
		}
		LastRegion = NULL;
		Regions = NULL;
	}
};

// We want to allow caching DLs that start at the same address but have different lengths,
// so the size has to be in the ID.
inline u64 CreateMapId(u32 address, u32 size)
{
	return ((u64)address << 32) | size;
}

inline u64 CreateVMapId(u32 VATUSED)
{
	u64 vmap_id = 0x9368e53c2f6af274ULL ^ g_VtxDesc.Hex;
	for(int i = 0; i < 8 ; i++)
	{
		if(VATUSED & (1 << i))
		{
			vmap_id ^= (((u64)g_VtxAttr[i].g0.Hex) | (((u64)g_VtxAttr[i].g1.Hex) << 32)) ^ (((u64)g_VtxAttr[i].g2.Hex) << i);
		}
	}
	for(int i = 0; i < 12; i++)
	{
		if(VATUSED & (1 << (i + 16)))
			vmap_id  ^= (((u64)arraybases[i]) ^ (((u64)arraystrides[i]) << 32));
	}
	return vmap_id;
}

typedef std::map<u64, CachedDisplayList> DLMap;

struct VDlist
{
	DLMap dl_map;
	u32 VATUsed;
	u32 count;
};

typedef std::map<u64, VDlist> VDLMap;

static VDLMap dl_map;
static u8* dlcode_cache;

static Gen::XEmitter emitter;



// First pass - analyze
u32 AnalyzeAndRunDisplayList(u32 address, u32 size, CachedDisplayList *dl)
{
	u8* old_pVideoData = g_pVideoData;
	u8* startAddress = Memory::GetPointer(address);
	u32 num_xf_reg = 0;
	u32 num_cp_reg = 0;
	u32 num_bp_reg = 0; 
	u32 num_index_xf = 0;
	u32 num_draw_call = 0;
	u32 result = 0;
	

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != 0)
	{
		g_pVideoData = startAddress;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		u8 *end = g_pVideoData + size;
		while (g_pVideoData < end)
		{
			// Yet another reimplementation of the DL reading...
			int cmd_byte = DataReadU8();
			switch (cmd_byte)
			{
			case GX_NOP:
				break;

			case GX_LOAD_CP_REG: //0x08
				{
					u8 sub_cmd = DataReadU8();
					u32 value = DataReadU32();
					LoadCPReg(sub_cmd, value);
					INCSTAT(stats.thisFrame.numCPLoads);
					num_cp_reg++;
				}
				break;

			case GX_LOAD_XF_REG:
				{
					u32 Cmd2 = DataReadU32();
					int transfer_size = ((Cmd2 >> 16) & 15) + 1;
					u32 xf_address = Cmd2 & 0xFFFF;
					GC_ALIGNED128(u32 data_buffer[16]);
					DataReadU32xFuncs[transfer_size-1](data_buffer);
					LoadXFReg(transfer_size, xf_address, data_buffer);
					INCSTAT(stats.thisFrame.numXFLoads);
					num_xf_reg++;
				}
				break;

			case GX_LOAD_INDX_A: //used for position matrices
				{
					LoadIndexedXF(DataReadU32(), 0xC);
					num_index_xf++;
				}
				break;
			case GX_LOAD_INDX_B: //used for normal matrices
				{
					LoadIndexedXF(DataReadU32(), 0xD);
					num_index_xf++;
				}
				break;
			case GX_LOAD_INDX_C: //used for postmatrices
				{
					LoadIndexedXF(DataReadU32(), 0xE);
					num_index_xf++;
				}
				break;
			case GX_LOAD_INDX_D: //used for lights
				{
					LoadIndexedXF(DataReadU32(), 0xF);
					num_index_xf++;
				}
				break;
			case GX_CMD_CALL_DL:
				{
					u32 addr = DataReadU32();
					u32 count = DataReadU32();
					ExecuteDisplayList(addr, count);
				}
				break;
			case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
				DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
				break;
			case GX_CMD_INVL_VC: // Invalidate Vertex Cache	
				DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
				break;
			case GX_LOAD_BP_REG: //0x61
				{
					u32 bp_cmd = DataReadU32();
					LoadBPReg(bp_cmd);
					INCSTAT(stats.thisFrame.numBPLoads);
					num_bp_reg++;
				}
				break;

			// draw primitives 
			default:
				if (cmd_byte & 0x80)
				{
					// load vertices (use computed vertex size from FifoCommandRunnable above)
					u16 numVertices = DataReadU16();
					if(numVertices > 0)
					{
						result |= 1 << (cmd_byte & GX_VAT_MASK);
						VertexLoaderManager::RunVertices(
							cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
							(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
							numVertices);
						num_draw_call++;
						const u32 tc[12] = {
							g_VtxDesc.Position, g_VtxDesc.Normal, g_VtxDesc.Color0, g_VtxDesc.Color1, g_VtxDesc.Tex0Coord, g_VtxDesc.Tex1Coord, 
							g_VtxDesc.Tex2Coord, g_VtxDesc.Tex3Coord, g_VtxDesc.Tex4Coord, g_VtxDesc.Tex5Coord, g_VtxDesc.Tex6Coord, (const u32)((g_VtxDesc.Hex >> 31) & 3)
						};
						for(int i = 0; i < 12; i++)
						{
							if(tc[i] > 1)
							{
								result |= 1 << (i + 16);
							}
						}
					}
				}
				else
				{
					ERROR_LOG(VIDEO, "OpcodeDecoding::Decode: Illegal command %02x", cmd_byte);
					break;
				}
				break;
			}
		}
		INCSTAT(stats.numDListsCalled);
		INCSTAT(stats.thisFrame.numDListsCalled);
		// un-swap
		Statistics::SwapDL();
	}
	dl->num_bp_reg = num_bp_reg;
	dl->num_cp_reg = num_cp_reg;
	dl->num_draw_call = num_draw_call;
	dl->num_index_xf = num_index_xf;
	dl->num_xf_reg = num_xf_reg;
	// reset to the old pointer
	g_pVideoData = old_pVideoData;	
	return result;
}

// The only sensible way to detect changes to vertex data is to convert several times 
// and hash the output.

// Second pass - compile
// Since some commands can affect the size of other commands, we really have no choice
// but to compile as we go, interpreting the list. We can't compile and then execute, we must
// compile AND execute at the same time. The second time the display list gets called, we already
// have the compiled code so we don't have to interpret anymore, we just run it.
void CompileAndRunDisplayList(u32 address, u32 size, CachedDisplayList *dl)
{
	u8* old_pVideoData = g_pVideoData;
	u8* startAddress = Memory::GetPointer(address);

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != 0)
	{
		g_pVideoData = startAddress;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		u8 *end = g_pVideoData + size;

		emitter.AlignCode4();
		dl->compiled_code = emitter.GetCodePtr();
		emitter.ABI_EmitPrologue(4);

		while (g_pVideoData < end)
		{
			// Yet another reimplementation of the DL reading...
			int cmd_byte = DataReadU8();
			switch (cmd_byte)
			{
			case GX_NOP:
				// Execute
				// Compile
				break;

			case GX_LOAD_CP_REG: //0x08
				{
					// Execute
					u8 sub_cmd = DataReadU8();
					u32 value = DataReadU32();
					LoadCPReg(sub_cmd, value);
					INCSTAT(stats.thisFrame.numCPLoads);

					// Compile
					emitter.ABI_CallFunctionCC((void *)&LoadCPReg, sub_cmd, value);
				}
				break;

			case GX_LOAD_XF_REG:
				{
					// Execute
					u32 Cmd2 = DataReadU32();
					int transfer_size = ((Cmd2 >> 16) & 15) + 1;
					u32 xf_address = Cmd2 & 0xFFFF;
					ReferencedDataRegion* NewRegion = new ReferencedDataRegion;
					NewRegion->MustClean = true;
					NewRegion->size = transfer_size * 4;
					NewRegion->start_address = (u8*) new u8[NewRegion->size+15+12];  // alignment and guaranteed space
					NewRegion->hash = 0;
					dl->InsertRegion(NewRegion);
					u32 *data_buffer = (u32*)(u8*)(((size_t)NewRegion->start_address+0xf)&~0xf);
					DataReadU32xFuncs[transfer_size-1](data_buffer);
					LoadXFReg(transfer_size, xf_address, data_buffer);
					INCSTAT(stats.thisFrame.numXFLoads);
					// Compile
					emitter.ABI_CallFunctionCCP((void *)&LoadXFReg, transfer_size, xf_address, data_buffer);
				}
				break;

			case GX_LOAD_INDX_A: //used for position matrices
				{
					u32 value = DataReadU32();
					// Execute
					LoadIndexedXF(value, 0xC);
					// Compile
					emitter.ABI_CallFunctionCC((void *)&LoadIndexedXF, value, 0xC);
				}
				break;
			case GX_LOAD_INDX_B: //used for normal matrices
				{
					u32 value = DataReadU32();
					// Execute
					LoadIndexedXF(value, 0xD);
					// Compile
					emitter.ABI_CallFunctionCC((void *)&LoadIndexedXF, value, 0xD);
				}
				break;
			case GX_LOAD_INDX_C: //used for postmatrices
				{
					u32 value = DataReadU32();
					// Execute
					LoadIndexedXF(value, 0xE);
					// Compile
					emitter.ABI_CallFunctionCC((void *)&LoadIndexedXF, value, 0xE);
				}
				break;
			case GX_LOAD_INDX_D: //used for lights
				{
					u32 value = DataReadU32();
					// Execute
					LoadIndexedXF(value, 0xF);
					// Compile
					emitter.ABI_CallFunctionCC((void *)&LoadIndexedXF, value, 0xF);
				}
				break;

			case GX_CMD_CALL_DL:
				{
					u32 addr= DataReadU32();
					u32 count = DataReadU32();
					ExecuteDisplayList(addr, count);
					emitter.ABI_CallFunctionCC((void *)&ExecuteDisplayList, addr, count);
				}
				break;

			case GX_CMD_UNKNOWN_METRICS:
				// zelda 4 swords calls it and checks the metrics registers after that
				break;

			case GX_CMD_INVL_VC:// Invalidate	(vertex cache?)	
				DEBUG_LOG(VIDEO, "Invalidate	(vertex cache?)");
				break;

			case GX_LOAD_BP_REG: //0x61
				{
					u32 bp_cmd = DataReadU32();
					// Execute
					LoadBPReg(bp_cmd);
					INCSTAT(stats.thisFrame.numBPLoads);
					// Compile
					emitter.ABI_CallFunctionC((void *)&LoadBPReg, bp_cmd);
				}
				break;

				// draw primitives 
			default:
				if (cmd_byte & 0x80)
				{
					// load vertices (use computed vertex size from FifoCommandRunnable above)
					u16 numVertices = DataReadU16();
					if(numVertices > 0)
					{
						// Execute
						u8* StartAddress = VertexManager::s_pBaseBufferPointer;
						VertexManager::Flush();
						VertexLoaderManager::RunVertices(
							cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
							(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
							numVertices);
						u32 Vdatasize = (u32)(VertexManager::s_pCurBufferPointer - StartAddress);
						if (Vdatasize > 0)
						{
							// Compile
							ReferencedDataRegion* NewRegion = new ReferencedDataRegion;
							NewRegion->MustClean = true;
							NewRegion->size = Vdatasize;
							NewRegion->start_address = (u8*)new u8[Vdatasize]; 
							NewRegion->hash = 0;
							dl->InsertRegion(NewRegion);
							memcpy(NewRegion->start_address, StartAddress, Vdatasize);
							emitter.ABI_CallFunctionCCCP((void *)&VertexLoaderManager::RunCompiledVertices, cmd_byte & GX_VAT_MASK, (cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT, numVertices, NewRegion->start_address);
							const u32 tc[12] = {
							g_VtxDesc.Position, g_VtxDesc.Normal, g_VtxDesc.Color0, g_VtxDesc.Color1, g_VtxDesc.Tex0Coord, g_VtxDesc.Tex1Coord, 
							g_VtxDesc.Tex2Coord, g_VtxDesc.Tex3Coord, g_VtxDesc.Tex4Coord, g_VtxDesc.Tex5Coord, g_VtxDesc.Tex6Coord, (const u32)((g_VtxDesc.Hex >> 31) & 3)
							};
							for(int i = 0; i < 12; i++)
							{
								if(tc[i] > 1)
								{
									u8* saddr = cached_arraybases[i];
									int arraySize = arraystrides[i] * ((tc[i] == 2)? numVertices : ((numVertices < 1024)? 2 * numVertices : numVertices));
									dl->InsertOverlapingRegion(saddr, arraySize);
								}
							}
						}
					}
				}
				else
				{
					ERROR_LOG(VIDEO, "DLCache::CompileAndRun: Illegal command %02x", cmd_byte);
					break;
				}
				break;
			}
		}
		emitter.ABI_EmitEpilogue(4);
		INCSTAT(stats.numDListsCalled);
		INCSTAT(stats.thisFrame.numDListsCalled);
		Statistics::SwapDL();
	}
	g_pVideoData = old_pVideoData;
}


void Init()
{
	CheckContextId = 0;
	dlcode_cache = (u8*)AllocateExecutableMemory(DL_CODE_CACHE_SIZE, false);  // Don't need low memory.
	emitter.SetCodePtr(dlcode_cache);
}

void Shutdown()
{
	Clear();
	FreeMemoryPages(dlcode_cache, DL_CODE_CACHE_SIZE);	
	dlcode_cache = NULL;
}

void Clear() 
{
	VDLMap::iterator iter = dl_map.begin();
	while (iter != dl_map.end())
	{
		VDlist &ParentEntry = iter->second;
		DLMap::iterator childiter = ParentEntry.dl_map.begin();
		while (childiter != ParentEntry.dl_map.end())
		{
			CachedDisplayList &entry = childiter->second;
			entry.ClearRegions();
			childiter++;
		}

		ParentEntry.dl_map.clear();
		iter++;
	}
	dl_map.clear();
	// Reset the cache pointers.
	emitter.SetCodePtr(dlcode_cache);
}

void ProgressiveCleanup()
{
	VDLMap::iterator iter = dl_map.begin();
	while (iter != dl_map.end())
	{
		VDlist &ParentEntry = iter->second;
		DLMap::iterator childiter = ParentEntry.dl_map.begin();
		while (childiter != ParentEntry.dl_map.end()) 
		{
			CachedDisplayList &entry = childiter->second;
			int limit = 3600;
			if (entry.frame_count < frameCount - limit)
			{
				entry.ClearRegions();
				ParentEntry.dl_map.erase(childiter++);  // (this is gcc standard!)
			}
			else
			{
				++childiter;
			}
		}

		if(ParentEntry.dl_map.empty())
		{
			dl_map.erase(iter++);
		}
		else
		{
			iter++;
		}
	}
}

static size_t GetSpaceLeft()
{
	return DL_CODE_CACHE_SIZE - (emitter.GetCodePtr() - dlcode_cache);
}

}  // namespace

// NOTE - outside the namespace on purpose.
bool HandleDisplayList(u32 address, u32 size)
{
	//Fixed DlistCaching now is fully functional still some things to workout
	if(!g_ActiveConfig.bDlistCachingEnable)
		return false;
	if(size == 0)
		return false;

	// TODO: Is this thread safe?
	if (DLCache::GetSpaceLeft() < DL_CODE_CLEAR_THRESHOLD)
	{
		DLCache::Clear();
	}

	u64 dl_id = DLCache::CreateMapId(address, size);
	u64 vhash = 0;
	DLCache::VDLMap::iterator Parentiter = DLCache::dl_map.find(dl_id);
	DLCache::DLMap::iterator iter;
	bool childexist = false;

	if (Parentiter != DLCache::dl_map.end())
	{
		vhash = DLCache::CreateVMapId(Parentiter->second.VATUsed);
		iter = 	Parentiter->second.dl_map.find(vhash);
		childexist = iter != Parentiter->second.dl_map.end();
	}

	if (Parentiter != DLCache::dl_map.end() && childexist)
	{
		DLCache::CachedDisplayList &dl = iter->second;
		if (dl.uncachable)
		{
			return false;
		}

		switch (dl.pass)
		{
		case DLCache::DLPASS_COMPILE:
			// First, check that the hash is the same as the last time.
			if (dl.dl_hash != GetHash64(Memory::GetPointer(address), size, 0))
			{
				dl.uncachable = true;
				return false;
			}
			DLCache::CompileAndRunDisplayList(address, size, &dl);
			dl.pass = DLCache::DLPASS_RUN;
			break;
		case DLCache::DLPASS_RUN:
			{
				bool DlistChanged = false;
				if (dl.check != CheckContextId)
				{
					dl.check = CheckContextId;
					DlistChanged = !dl.CheckRegions() || dl.dl_hash != GetHash64(Memory::GetPointer(address), size, 0);					
				}
				if (DlistChanged) 
				{
					dl.uncachable = true;
					dl.ClearRegions();
					return false;
				}
				dl.frame_count= frameCount;
				u8 *old_datareader = g_pVideoData;
				((void (*)())(void*)(dl.compiled_code))();
				Statistics::SwapDL();
				ADDSTAT(stats.thisFrame.numCPLoadsInDL, dl.num_cp_reg);
				ADDSTAT(stats.thisFrame.numXFLoadsInDL, dl.num_xf_reg);
				ADDSTAT(stats.thisFrame.numBPLoadsInDL, dl.num_bp_reg);

				ADDSTAT(stats.thisFrame.numCPLoads, dl.num_cp_reg);
				ADDSTAT(stats.thisFrame.numXFLoads, dl.num_xf_reg);
				ADDSTAT(stats.thisFrame.numBPLoads, dl.num_bp_reg);

				INCSTAT(stats.numDListsCalled);
				INCSTAT(stats.thisFrame.numDListsCalled);
				
				Statistics::SwapDL();
				g_pVideoData = old_datareader;
				break;
			}
		}
		return true;
	}

	DLCache::CachedDisplayList dl;
	
	u32 dlvatused = DLCache::AnalyzeAndRunDisplayList(address, size, &dl);
	dl.dl_hash = GetHash64(Memory::GetPointer(address), size,0);
	dl.pass = DLCache::DLPASS_COMPILE;
	dl.check = CheckContextId;
	vhash = DLCache::CreateVMapId(dlvatused);
	if(Parentiter != DLCache::dl_map.end())
	{
		DLCache::VDlist &vdl = Parentiter->second;
		vdl.dl_map[vhash] = dl;
		vdl.VATUsed = dlvatused; 
		vdl.count++;
	}
	else
	{
		DLCache::VDlist vdl;
		vdl.dl_map[vhash] = dl;
		vdl.VATUsed = dlvatused; 
		vdl.count = 1;
		DLCache::dl_map[dl_id] = vdl;
	}

	return true;
}

void IncrementCheckContextId()
{
	CheckContextId++;
}
