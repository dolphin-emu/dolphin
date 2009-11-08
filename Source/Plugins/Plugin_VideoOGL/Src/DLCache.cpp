// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// TODO: Handle cache-is-full condition :p

#include <map>

#include "Common.h"
#include "VideoCommon.h"
#include "Hash.h"
#include "MemoryUtil.h"
#include "DataReader.h"
#include "Statistics.h"
#include "OpcodeDecoding.h"   // For the GX_ constants.

#include "XFMemory.h"
#include "CPMemory.h"
#include "BPMemory.h"

#include "VertexManager.h"
#include "VertexLoaderManager.h"

#include "x64Emitter.h"
#include "ABI.h"

#include "DLCache.h"

#define DL_CODE_CACHE_SIZE (1024*1024*16)
#define DL_STATIC_DATA_SIZE (1024*1024*4)
extern int frameCount;

using namespace Gen;

namespace DLCache
{

// Currently just recompiles the DLs themselves, doesn't bother with the vertex data.
// The speed boost is pretty small. The real big boost will come when we also store
// vertex arrays in the cached DLs.

enum DisplayListPass {
	DLPASS_ANALYZE,
	DLPASS_COMPILE,
	DLPASS_RUN,
};

struct VDataHashRegion
{
	u32 hash;
	u32 start_address;
	int size;
};

struct CachedDisplayList
{
	CachedDisplayList()
		: uncachable(false),
		  pass(DLPASS_ANALYZE),
		  next_check(1)
	{
		frame_count = frameCount;
	}

	bool uncachable;  // if set, this DL will always be interpreted. This gets set if hash ever changes.

	int pass;
	u32 dl_hash;

	int check;
	int next_check;

	u32 vdata_hash;

	std::vector<VDataHashRegion> hash_regions;

	int frame_count;

	// ... Something containing cached vertex buffers here ...

	// Compile the commands themselves down to native code.
	const u8 *compiled_code;
};

// We want to allow caching DLs that start at the same address but have different lengths,
// so the size has to be in the ID.
inline u64 CreateMapId(u32 address, u32 size)
{
	return ((u64)address << 32) | size;
}

typedef std::map<u64, CachedDisplayList> DLMap;

static DLMap dl_map;
static u8 *dlcode_cache;
static u8 *static_data_buffer;
static u8 *static_data_ptr;

static Gen::XEmitter emitter;

// Everything gets free'd when the cache is cleared.
u8 *AllocStaticData(int size)
{
	u8 *cur_ptr = static_data_ptr;
	static_data_ptr += (size + 3) & ~3;
	return cur_ptr;
}

// First pass - analyze
bool AnalyzeAndRunDisplayList(u32 address, int	 size, CachedDisplayList *dl)
{
	int num_xf_reg = 0;
	int num_cp_reg = 0;
	int num_bp_reg = 0;
	int num_index_xf = 0;
	int num_draw_call = 0;

	u8 *old_datareader = g_pVideoData;
	g_pVideoData = Memory_GetPtr(address);

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
				// Execute
				u8 sub_cmd = DataReadU8();
				u32 value = DataReadU32();
				LoadCPReg(sub_cmd, value);
				INCSTAT(stats.thisFrame.numCPLoads);

				// Analyze
				num_cp_reg++;
			}
			break;

		case GX_LOAD_XF_REG:
			{
				// Execute
				u32 Cmd2 = DataReadU32();
				int transfer_size = ((Cmd2 >> 16) & 15) + 1;
				u32 xf_address = Cmd2 & 0xFFFF;
				// TODO - speed this up. pshufb?
				u32 data_buffer[16];
				for (int i = 0; i < transfer_size; i++)
					data_buffer[i] = DataReadU32();
				LoadXFReg(transfer_size, xf_address, data_buffer);
				INCSTAT(stats.thisFrame.numXFLoads);

				// Analyze
				num_xf_reg++;
			}
			break;

		case GX_LOAD_INDX_A: //used for position matrices
			{
				u32 value = DataReadU32();
				// Execute
				LoadIndexedXF(value, 0xC);
				// Analyze
				num_index_xf++;
			}
			break;
		case GX_LOAD_INDX_B: //used for normal matrices
			{
				u32 value = DataReadU32();
				// Execute
				LoadIndexedXF(value, 0xD);
				// Analyze
				num_index_xf++;
			}
			break;
		case GX_LOAD_INDX_C: //used for postmatrices
			{
				u32 value = DataReadU32();
				// Execute
				LoadIndexedXF(value, 0xE);
				// Analyze
				num_index_xf++;
			}
			break;
		case GX_LOAD_INDX_D: //used for lights
			{
				u32 value = DataReadU32();
				// Execute
				LoadIndexedXF(value, 0xF);
				// Analyze
				num_index_xf++;
			}
			break;

		case GX_CMD_CALL_DL:
			PanicAlert("Seeing DL call inside DL.");
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

				// Analyze
			}
			break;

			// draw primitives 
		default:
			if (cmd_byte & 0x80)
			{
				// load vertices (use computed vertex size from FifoCommandRunnable above)

				// Execute
				u16 numVertices = DataReadU16();

				VertexLoaderManager::RunVertices(
					cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
					(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
					numVertices);

				// Analyze
			}
			else
			{
				ERROR_LOG(VIDEO, "DLCache::CompileAndRun: Illegal command %02x", cmd_byte);
				break;
			}
			break;
		}
	}

	g_pVideoData = old_datareader;
	return true;
}

// The only sensible way to detect changes to vertex data is to convert several times 
// and hash the output.

// Second pass - compile
// Since some commands can affect the size of other commands, we really have no choice
// but to compile as we go, interpreting the list. We can't compile and then execute, we must
// compile AND execute at the same time. The second time the display list gets called, we already
// have the compiled code so we don't have to interpret anymore, we just run it.
bool CompileAndRunDisplayList(u32 address, int size, CachedDisplayList *dl)
{
	VertexManager::Flush();

	u8 *old_datareader = g_pVideoData;
	g_pVideoData = Memory_GetPtr(address);
	
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
				// TODO - speed this up. pshufb?
				u8 *real_data_buffer = AllocStaticData(4 * transfer_size);
				u32 *data_buffer = (u32 *)real_data_buffer;
				for (int i = 0; i < transfer_size; i++)
					data_buffer[i] = DataReadU32();
				LoadXFReg(transfer_size, xf_address, data_buffer);
				INCSTAT(stats.thisFrame.numXFLoads);

				// Compile
				emitter.ABI_CallFunctionCCP((void *)&LoadXFReg, transfer_size, address, data_buffer);
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
			PanicAlert("Seeing DL call inside DL.");
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

				// Execute
				u16 numVertices = DataReadU16();

				u64 pre_draw_video_data = (u64)g_pVideoData;

				VertexLoaderManager::RunVertices(
					cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
					(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
					numVertices);

				// Compile
#ifdef _M_X64
				emitter.MOV(64, R(RAX), Imm64(pre_draw_video_data));
				emitter.MOV(64, M(&g_pVideoData), R(RAX));
#else
				emitter.MOV(32, R(EAX), Imm32((u32)pre_draw_video_data));
				emitter.MOV(32, M(&g_pVideoData), R(EAX));
#endif
				emitter.ABI_CallFunctionCCC(
					(void *)&VertexLoaderManager::RunVertices,
					cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
					(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
					numVertices);
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

	g_pVideoData = old_datareader;
	return true;
}

// This one's pretty expensive. We should check if we can get away with only
// hashing the entire DL the first 3 frames or something.
u32 ComputeDLHash(u32 address, u32 size)
{
	u8 *ptr = Memory_GetPtr(address);
	return HashFletcher(ptr, size & ~1);
}

void Init()
{
	dlcode_cache = (u8 *)AllocateExecutableMemory(DL_CODE_CACHE_SIZE, false);  // Don't need low memory.
	static_data_buffer = (u8 *)AllocateMemoryPages(DL_STATIC_DATA_SIZE);
	static_data_ptr = static_data_buffer;
	emitter.SetCodePtr(dlcode_cache);
}

void Shutdown()
{
	Clear();
	FreeMemoryPages(dlcode_cache, DL_CODE_CACHE_SIZE);
	FreeMemoryPages(static_data_buffer, DL_STATIC_DATA_SIZE);
	dlcode_cache = NULL;
}

void Clear() 
{
	dl_map.clear();

	// Reset the cache pointers.
	emitter.SetCodePtr(dlcode_cache);
	static_data_ptr = static_data_buffer;
}

void ProgressiveCleanup()
{
	DLMap::iterator iter = dl_map.begin();
	while (iter != dl_map.end()) {
		CachedDisplayList &entry = iter->second;
		int limit = iter->second.uncachable ? 1200 : 400;
		if (entry.frame_count < frameCount - limit) {
			// entry.Destroy();
#ifdef _WIN32
			iter = dl_map.erase(iter);
#else
			dl_map.erase(iter++);  // (this is gcc standard!)
#endif
		}
		else
			iter++;
	}
}

}  // namespace

// NOTE - outside the namespace on purpose.
bool HandleDisplayList(u32 address, u32 size)
{
	// Disable display list caching since the benefit isn't much to write home about
	// right now...
	return false;

	u64 dl_id = DLCache::CreateMapId(address, size);
	DLCache::DLMap::iterator iter = DLCache::dl_map.find(dl_id);

	stats.numDListsAlive = (int)DLCache::dl_map.size();
	if (iter != DLCache::dl_map.end())
	{
		DLCache::CachedDisplayList &dl = iter->second;
		if (dl.uncachable)
		{
			// We haven't compiled it - let's return false so it gets
			// interpreted.
			return false;
		}

		// Got one! And it's been compiled too, so let's run the compiled code!
		switch (dl.pass)
		{
		case DLCache::DLPASS_ANALYZE:
			PanicAlert("DLPASS_ANALYZE - should have been done the first pass");
			break;
		case DLCache::DLPASS_COMPILE:
			// First, check that the hash is the same as the last time.
			if (dl.dl_hash != HashAdler32(Memory_GetPtr(address), size))
			{
				// PanicAlert("uncachable %08x", address);
				dl.uncachable = true;
				return false;
			}
			DLCache::CompileAndRunDisplayList(address, size, &dl);
			dl.pass = DLCache::DLPASS_RUN;
			break;
		case DLCache::DLPASS_RUN:
			{
				// Every N draws, check hash
				dl.check--;
				if (dl.check <= 0)
				{
					if (dl.dl_hash != HashAdler32(Memory_GetPtr(address), size)) 
					{
						dl.uncachable = true;
						return false;
					}
					dl.check = dl.next_check;
					dl.next_check *= 2;
					if (dl.next_check > 1024)
						dl.next_check = 1024;
				}
				u8 *old_datareader = g_pVideoData;
				((void (*)())(void*)(dl.compiled_code))();
				g_pVideoData = old_datareader;
				break;
			}
		}
		return true;
	}

	DLCache::CachedDisplayList dl;
	
	if (DLCache::AnalyzeAndRunDisplayList(address, size, &dl)) {
		dl.dl_hash = HashAdler32(Memory_GetPtr(address), size);
		dl.pass = DLCache::DLPASS_COMPILE;
		dl.check = 1;
		dl.next_check = 1;
		DLCache::dl_map[dl_id] = dl;
		return true;
	} else {
		dl.uncachable = true;
		DLCache::dl_map[dl_id] = dl;
		return true;  // don't also interpret the list.
	}
}
