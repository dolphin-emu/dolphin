// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GeckoCode.h"

#include "Thread.h"
#include "HW/Memmap.h"
#include "ConfigManager.h"

#include "vector"
#include "PowerPC/PowerPC.h"
#include "CommonPaths.h"

namespace Gecko
{

enum
{
	// Code Types
	CODETYPE_WRITE_FILL =		0x0,
	CODETYPE_IF =				0x1,
	CODETYPE_BA_PO_OPS =		0x2,
	CODETYPE_FLOW_CONTROL =		0x3,
	CODETYPE_REGISTER_OPS =		0x4,
	CODETYPE_SPECIAL_IF =		0x5,
	CODETYPE_ASM_SWITCH_RANGE =	0x6,
	CODETYPE_END_CODES =		0x7,

	// Data Types
	DATATYPE_8BIT =		0x0,
	DATATYPE_16BIT =	0x1,
	DATATYPE_32BIT =	0x2,
};

// globals
static u32 base_address = 0;
static u32 pointer_address = 0;
static u32 gecko_register[0x10] = {0};
static struct
{
	u32 address;
	u32 number;
} block[0x10];

// codes execute when counter is 0
static int	code_execution_counter = 0;

// Track whether the code handler has been installed
static bool	code_handler_installed = false;

// the currently active codes
std::vector<GeckoCode>	active_codes;

// return true if code execution is on
inline bool CodeExecution()
{
	return (0 == code_execution_counter);
}

u32 GeckoCode::Code::GetAddress() const
{
	return gcaddress + (use_po ? pointer_address : (base_address & 0xFE000000));
}

// return true if a code exists
bool GeckoCode::Exist(u32 address, u32 data)
{
	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = codes.begin(),
		codes_end = codes.end();
	for (; codes_iter != codes_end; ++codes_iter)
	{
		if (codes_iter->address == address && codes_iter->data == data)
			return true;
	}
	return false;
}

// return true if the code is identical
bool GeckoCode::Compare(GeckoCode compare) const
{
	if (codes.size() != compare.codes.size())
		return false;

	unsigned int exist = 0;
	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = codes.begin(),
		codes_end = codes.end();
	for (; codes_iter != codes_end; ++codes_iter)
	{
		if (compare.Exist(codes_iter->address, codes_iter->data))
			exist++;
	}
	return exist == codes.size();
}

static std::mutex active_codes_lock;

// currently running code
static GeckoCode::Code *codes_start = NULL, *current_code = NULL;
static const GeckoCode::Code *codes_end = NULL;

// asm codes used for CT6 CST1
static std::map<u32, std::vector<u32> > inserted_asm_codes;

// Functions for each code type
bool RamWriteAndFill();
bool RegularIf();
bool BaPoOps();
bool FlowControl();
bool RegisterOps();
bool SpecialIf();
bool AsmSwitchRange();
bool EndCodes();

bool MathOperation(u32& ret, const u32 left, const u32 right, const u8 type);

void SetActiveCodes(const std::vector<GeckoCode>& gcodes)
{
	std::lock_guard<std::mutex> lk(active_codes_lock);

	active_codes.clear();
	// add enabled codes
	std::vector<GeckoCode>::const_iterator
		gcodes_iter = gcodes.begin(),
		gcodes_end = gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		if (gcodes_iter->enabled)
		{
			// TODO: apply modifiers
			// TODO: don't need description or creator string, just takin up memory
			active_codes.push_back(*gcodes_iter);
		}
	}

	inserted_asm_codes.clear();

	code_handler_installed = false;
}

bool InstallCodeHandler()
{
	u32 codelist_location = 0x800028B8; // Debugger on location (0x800022A8 = Debugger off, using codehandleronly.bin)
	std::string data;
	std::string _rCodeHandlerFilename = File::GetSysDirectory() + GECKO_CODE_HANDLER;
	if (!File::ReadFileToString(false, _rCodeHandlerFilename.c_str(), data))
		return false;

	// Install code handler
	Memory::WriteBigEData((const u8*)data.data(), 0x80001800, data.length());

	// Turn off Pause on start
	Memory::Write_U32(0, 0x80002774);

	// Write the Game ID into the location expected by WiiRD
	Memory::WriteBigEData(Memory::GetPointer(0x80000000), 0x80001800, 6);

	// Create GCT in memory
	Memory::Write_U32(0x00d0c0de, codelist_location);
	Memory::Write_U32(0x00d0c0de, codelist_location + 4);

	std::lock_guard<std::mutex> lk(active_codes_lock);

	int i = 0;
	std::vector<GeckoCode>::iterator
		gcodes_iter = active_codes.begin(),
		gcodes_end = active_codes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		if (gcodes_iter->enabled)
		{
			current_code = codes_start = &*gcodes_iter->codes.begin();
			codes_end = &*gcodes_iter->codes.end();
			for (; current_code < codes_end; ++current_code)
			{
				const GeckoCode::Code& code = *current_code;

				// Make sure we have enough memory to hold the code list
				if ((codelist_location + 24 + i) < 0x80003000)
				{
					Memory::Write_U32(code.address, codelist_location + 8 + i);
					Memory::Write_U32(code.data, codelist_location + 12 + i);
					i += 8;
				}
			}
		}
	}

	Memory::Write_U32(0xff000000, codelist_location + 8 + i);
	Memory::Write_U32(0x00000000, codelist_location + 12 + i);

	// Turn on codes
	Memory::Write_U8(1, 0x80001807);

	// Invalidate the icache
	for (unsigned int j = 0; j < data.length(); j += 32)
	{
		PowerPC::ppcState.iCache.Invalidate(0x80001800 + j);
	}
	return true;
}

bool RunGeckoCode(GeckoCode& gecko_code)
{
	static bool (*code_type_funcs[])(void) =
	{ RamWriteAndFill, RegularIf, BaPoOps, FlowControl, RegisterOps, SpecialIf, AsmSwitchRange, EndCodes };

	base_address = 0x80000000;
	pointer_address = 0x80000000;
	code_execution_counter = 0;

	current_code = codes_start = &*gecko_code.codes.begin();
	codes_end = &*gecko_code.codes.end();
	for (; current_code < codes_end; ++current_code)
	{
		const GeckoCode::Code& code = *current_code;

		bool result = true;

		switch (code.type)
		{
			// These codetypes run even if code_execution is off
		case CODETYPE_IF :
		case CODETYPE_FLOW_CONTROL :
		case CODETYPE_SPECIAL_IF :
		case CODETYPE_ASM_SWITCH_RANGE :
		case CODETYPE_END_CODES :
			result = code_type_funcs[code.type]();
			break;

			// The rest of the codetypes only run if code_execution is on
		default :
			if (CodeExecution())
				result = code_type_funcs[code.type]();
			break;
		}

		// code failed
		if (false == result)
		{
			// disable code to stop annoying error messages
			gecko_code.enabled = false;

			PanicAlertT("GeckoCode failed to run (CT%i CST%i) (%s)"
				"\n(either a bad code or the code type is not yet supported. Try using the native code handler by placing the codehandler.bin file into the Sys directory and restarting Dolphin.)"
				, code.type, code.subtype, gecko_code.name.c_str());
			return false;
		}
	}

	return true;
}

bool RunActiveCodes()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats)
	{
		std::lock_guard<std::mutex> lk(active_codes_lock);

		std::vector<GeckoCode>::iterator
			gcodes_iter = active_codes.begin(),
			gcodes_end = active_codes.end();
		for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
		{
			if (gcodes_iter->enabled)
				RunGeckoCode(*gcodes_iter);
			// we don't need to stop all codes if one fails, maybe
			//if (false == RunGeckoCode(*gcodes_iter))
				//return false;
		}
	}

	return true;
}

void RunCodeHandler()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats && active_codes.size() > 0)
	{
		u8 *gameId = Memory::GetPointer(0x80000000);
		u8 *wiirdId = Memory::GetPointer(0x80001800);

		if (!code_handler_installed || memcmp(gameId, wiirdId, 6))
			code_handler_installed = InstallCodeHandler();

		if (code_handler_installed)
		{
			if (PC == LR)
			{
				u32 oldLR = LR;
				PowerPC::CoreMode oldMode = PowerPC::GetMode();

				PC = 0x800018A8;
				LR = 0;

				// Execute the code handler in interpreter mode to track when it exits
				PowerPC::SetMode(PowerPC::MODE_INTERPRETER);

				while (PC != 0)
					PowerPC::SingleStep();

				PowerPC::SetMode(oldMode);
				PC = LR = oldLR;
			}
		}
		else
		{
			// Use the emulated code handler
			Gecko::RunActiveCodes();
		}
	}
}

const std::map<u32, std::vector<u32> >& GetInsertedAsmCodes()
{
	return inserted_asm_codes;
}

// CT0: Direct ram write/fill
// COMPLETE, maybe
bool RamWriteAndFill()
{
	const GeckoCode::Code& code = *current_code;
	u32 new_addr = code.GetAddress();
	const u32& data = code.data;

	u32 count = (data >> 16) + 1;	// note: +1

	switch (code.subtype)
	{
		// CST0: 8bits Write & Fill
	case DATATYPE_8BIT :
		while (count--)
		{
			Memory::Write_U8((u8)data, new_addr);
			++new_addr;
		}
		break;

		// CST1: 16bits Write & Fill
	case DATATYPE_16BIT :
		while (count--)
		{
			Memory::Write_U16((u16)data, new_addr);
			new_addr += 2;
		}
		break;

		// CST2: 32bits Write
	case DATATYPE_32BIT :
		Memory::Write_U32((u32)data, new_addr);
		break;

		// CST3: String Code
	case 0x3 :
		count = code.data;	// count is different from the other subtypes
		while (count)
		{
			if (codes_end == ++current_code)
				return false;

			// write bytes from address
			int byte_num = 4;
			while (byte_num-- && count)
			{
				Memory::Write_U8((u8)(current_code->address >> byte_num * 8), new_addr);
				++new_addr;
				--count;
			}

			// write bytes from data
			byte_num = 4;
			while (byte_num-- && count)
			{
				Memory::Write_U8((u8)(current_code->data >> byte_num * 8), new_addr);
				++new_addr;
				--count;
			}
		}
		break;

		// CST4: Serial Code
	case 0x4 :
	{
		if (codes_end == ++current_code)
			return false;
		u32 new_data = data;	// starting value of data
		const u8 data_type = current_code->address >> 28;
		const u32 data_inc = current_code->data;	// amount to increment the data
		const u16 addr_inc = (u16)current_code->address;	// amount to increment the address
		count = ((current_code->address >> 16) & 0xFFF) + 1;	// count is different from the other subtypes, note: +1
		while (count--)
		{
			// switch inside the loop, :/ o well
			switch (data_type)
			{
			case DATATYPE_8BIT :
				Memory::Write_U8((u8)new_data, new_addr);
				new_data = (u8)new_data + (u8)data_inc;
				break;

			case DATATYPE_16BIT :
				Memory::Write_U16((u16)new_data, new_addr);
				new_data = (u16)new_data + (u16)data_inc;
				break;

			case DATATYPE_32BIT :
				Memory::Write_U32((u32)new_data, new_addr);
				new_data += data_inc;
				break;

				// INVALID DATATYPE
			default :
				return false;
				break;
			}

			new_addr += addr_inc;
		}
	}
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;
	}

	return true;
}

// CT1 Regular If codes (16/32 bits)
// COMPLETE
bool RegularIf()
{
	const GeckoCode::Code& code = *current_code;

	const bool is_endif = !!(code.address & 0x1);

	// if code_execution is off and this is an endif, decrease the execution counter
	if (false == CodeExecution())
	{
		if (is_endif)
			--code_execution_counter;
	}

	bool result = false;

	// if code_execution is on, execute the conditional
	if (CodeExecution())
	{
		const u32 new_addr = code.GetAddress() & ~0x1;
		const u32& data = code.data;

		s32 read_value = 0;
		s32 data_value = 0;

		// 16bit vs 32bit
		if (code.subtype & 0x4)
		{
			// 16bits
			read_value = Memory::Read_U16(new_addr) & ~(data >> 16);
			data_value = (data & 0xFFFF);
		}
		else
		{
			// 32bits
			read_value = Memory::Read_U32(new_addr);
			data_value = data;
		}

		switch (code.subtype & 0x3)
		{
			// CST0 : 32bits (endif, then) If equal
			// CST4 : 16bits (endif, then) If equal
		case 0x0 :
			result = (read_value == data_value);
			break;

			// CST1 : 32bits (endif, then) If not equal
			// CST5 : 16bits (endif, then) If not equal
		case 0x1 :
			result = (read_value != data_value);
			break;

			// CST2 : 32bits (endif, then) If greater
			// CST6 : 16bits (endif, then) If greater
		case 0x2 :
			result = (read_value > data_value);
			break;

			// CST3 : 32bits (endif, then) If lower
			// CST7 : 16bits (endif, then) If lower
		case 0x3 :
			result = (read_value < data_value);
			break;
		}
	}

	// if the conditional returned false, or it never ran because execution is off,
	// increase the code execution counter
	if (false == result)
		++code_execution_counter;

	return true;
}

// CT2 Base Address/Pointer Operations
// NOT COMPLETE, last 2 subtypes aren't done
bool BaPoOps()
{
	const GeckoCode::Code& code = *current_code;

	// base_address vs pointer (ba vs po)
	u32& change_address = (code.subtype & 0x4) ? pointer_address : base_address;

	// 4STYZ00N XXXXXXXX
	u32 new_data = code.data;

	// append grN
	if (code.z)
		new_data += gecko_register[code.n];

	// append ba or po (depending on last nibble's first bit)
	if (code.y)
		new_data += (code.use_po ? pointer_address : base_address);

	// append to current value (not used in all subtypes, T will be 0 in those)
	if (code.t)
		new_data += change_address;

	switch (code.subtype & 0x3)
	{
		// CST0 : Load into Base Address
		// CST4 : Load into Pointer
	case 0x0 :
		change_address = Memory::Read_U32(new_data);
		break;

		// CST1 : Set Base Address to
		// CST5 : Set Pointer to
	case 0x1 :
		change_address = new_data;
		break;

		// CST2 : Save Base Address to
		// CST6 : Save Pointer to
	case 0x2 :
		Memory::Write_U32(change_address, new_data);
		break;

		// CST3 : Put next line of code location into the Base Address
		// CST7 : Put grN's location into the Pointer

		// conflicting documentation, one doc says CST7 is the same as CST3 but with po
	case 0x3 :
		// TODO:
		return false;	//
		break;
	}

	return true;
}

// CT3 Repeat/Goto/Gosub/Return
// COMPLETE, maybe
bool FlowControl()
{
	const GeckoCode::Code& code = *current_code;

	// only the return subtype runs when code execution is off
	if (false == CodeExecution() && code.subtype != 0x2)
		return true;

	// not all of these are used in all subtypes
	const u8 block_num = (u8)(code.data & 0xF);

	switch (code.subtype)
	{
		// CST0 : Set Repeat
	case 0x0 :
		block[block_num].number = code.address & 0xFFFF;
		block[block_num].address = (u32)(current_code - codes_start + 1);
		break;

		// CST1 : Execute Repeat
	case 0x1 :
		if (block[block_num].number)
		{
			--block[block_num].number;
			// needs -1 cause iterator gets ++ after code runs
			current_code = codes_start + block[block_num].address - 1;
		}
		break;

		// CST2 : Return
	case 0x2 :
		if (((code.address >> 20) & 0xF) ^ (u32)CodeExecution())
		{
			// needs -1 cause iterator gets ++ after code runs
			current_code = codes_start + block[block_num].address - 1;
		}
		break;

		// CST3 : Goto
	case 0x3 :
		if (((code.address >> 20) & 0xF) ^ (u32)CodeExecution())
		{
			GeckoCode::Code* const target_code = current_code + (s16)(code.address & 0xFFFF);

			if (target_code >= codes_start && target_code < codes_end)
			{
				// needs -1 cause iterator gets ++ after code runs
				current_code = target_code - 1;
			}
			else
			{
				return false;	// trying to GOTO to bad address
			}
		}
		break;

		// CST4 : Gosub
	case 0x4 :
		if (((code.address >> 20) & 0xF) ^ (u32)CodeExecution())
		{
			GeckoCode::Code* const target_code = current_code + (s16)(code.address & 0xFFFF);

			if (target_code >= codes_start && target_code < codes_end)
			{
				block[block_num].address = u32(current_code - codes_start + 1);
				// needs -1 cause iterator gets ++ after code runs
				current_code = target_code - 1;
			}
			else
			{
				return false;	// trying to GOSUB to bad address
			}
		}
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;
	}

	return true;
}

// CT4 Gecko Register Operations
// COMPLETE, maybe
bool RegisterOps()
{
	const GeckoCode::Code& code = *current_code;

	// 80TYZZZN XXXXXXXX

	u32 new_data = code.data;

	// append ba or po (depending on last nibble's first bit)
	if (code.y)
		new_data += (code.use_po ? pointer_address : base_address);

	u32& geckreg = gecko_register[code.n];

	switch (code.subtype)
	{
		// CST0 : Set Gecko Register to
	case 0x0 :
		// append to or set register
		geckreg = new_data + (code.t ? geckreg : 0);
		break;

		// CST1 : Load into Gecko Register
	case 0x1 :
		switch (code.t)
		{
			// 8bit
		case DATATYPE_8BIT :
			geckreg = Memory::Read_U8(new_data);
			break;

			// 16bit
		case DATATYPE_16BIT :
			geckreg = Memory::Read_U16(new_data);
			break;

			// 32bit
		case DATATYPE_32BIT :
			geckreg = Memory::Read_U32(new_data);
			break;

			// INVALID DATATYPE
		default :
			return false;
			break;
		}
		break;

		// CST2 : Save Gecko Register to
	case 0x2 :
		switch (code.t)
		{
			// 8bit
		case DATATYPE_8BIT :
			for (u16 i = 0; i <= code.z; ++i)
				Memory::Write_U8((u8)geckreg, new_data + i);
			break;

			// 16bit
		case DATATYPE_16BIT :
			for (u16 i = 0; i <= code.z; ++i)
				Memory::Write_U16((u16)geckreg, new_data + (i << 1));
			break;

			// 32bit
		case DATATYPE_32BIT :
			for (u16 i = 0; i <= code.z; ++i)
				Memory::Write_U32((u32)geckreg, new_data + (i << 2));
			break;

			// INVALID DATATYPE
		default :
			return false;
			break;
		}
		break;

		// CST3 : Gecko Register / Direct Value operations
		// CST4 : Gecko Registers operations
	case 0x3 :
	case 0x4 :
	{
		// subtype 3 uses value in .data subtype 4 uses register
		u32 right_val = (code.subtype & 0x1 ? code.data : gecko_register[code.data & 0xF]);
		if (code.y & 0x2)
			right_val = Memory::Read_U32(right_val);

		u32 left_val = geckreg;
		if (code.y & 0x1)
			left_val = Memory::Read_U32(left_val);

		if (false == MathOperation(geckreg, left_val, right_val, code.t))
			return false;
	}
		break;

		// CST5 : Memory Copy 1
		// CST6 : Memory Copy 2
	case 0x5 :
	case 0x6 :
	{
		const u8 src_gr = (code.z & 0xF);
		const u8 dst_gr = (code.n);

		u16 count = (u16)(code.address >> 8);

		// docs don't specify allowing 0xF for source and dest, but it can't hurt
		u32 src_addr = ((0xF == src_gr) ? (code.use_po ? pointer_address : base_address) : gecko_register[src_gr]);
		u32 dst_addr = ((0xF == dst_gr) ? (code.use_po ? pointer_address : base_address) : gecko_register[dst_gr]);

		if (0x5 == code.subtype)
			dst_addr += new_data;
		else
			src_addr += new_data;
		
		while (count--)
			Memory::Write_U8(Memory::Read_U8(src_addr++), dst_addr++);
	}
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;

	}

	return true;
}

// NOT COMPLETE, at least one op needs to be coded/fixed
bool MathOperation(u32& ret, const u32 left, const u32 right, const u8 type)
{
	// ? = T :
	switch (type)
	{
		// 0 : add (+)
	case 0x0 :
		ret = left + right;
		break;

		// 1 : mul (*)
	case 0x1 :
		ret = left * right;
		break;

		// 2 : or (|)
	case 0x2 :
		ret = left | right;
		break;

		// 3 : and (&)
	case 0x3 :
		ret = left & right;
		break;

		// 4 : xor (^)
	case 0x4 :
		ret = left ^ right;
		break;

		// 5 : slw (<<)
	case 0x5 :
		ret = left << right;
		break;

		// 6 : srw (>>)
	case 0x6 :
		ret = left >> right;
		break;

		// TODO: this one good?
		// 7 : rol (rotate left)
	case 0x7 :
		ret = (left << right) | (left >> (8 - right));
		break;

		// 8 : asr (arithmetic shift right)
	case 0x8 :
		ret = (s32)left >> right;
		break;

		// TODO: these float ops good?
		// A : fadds (single float add)
	case 0x9 :
		*(float*)&ret = *(float*)&left + *(float*)&right;
		break;

		// B : fmuls (single float mul)
	case 0xA :
		*(float*)&ret = *(float*)&left * *(float*)&right;
		break;

		// INVALID OPERATION TYPE
	default :
		return false;
		break;
	}

	return true;
}

// CT5: Special If codes (16bits)
// COMPLETE, maybe (ugly)
bool SpecialIf()
{
	// counter can modify the code :/
	GeckoCode::Code& code = *current_code;

	const bool is_endif = !!(code.address & 0x1);

	// if code_execution is off and this is an endif, decrease the execution counter
	if (false == CodeExecution())
	{
		if (is_endif)
			--code_execution_counter;
	}

	bool result = false;

	// TODO: should these be signed? probably?
	s16 left_val = 0, right_val = 0;

	// if code_execution is on, execute the conditional
	if (CodeExecution())
	{
		const u32 addr = code.GetAddress() & ~0x1;
		const u32& data = code.data;

		if (code.subtype ^ 0x4)
		{
			// CT5 Part1 : Unknown values comparison
			// A-______ NM00YYYY

			const u8 n = (u8)(data >> 28);
			const u8 m = (u8)((data >> 24) & 0xF);
			const u16 y = (u16)data;

			left_val = Memory::Read_U16(((0xF == n) ? addr : gecko_register[n]) & ~y);
			right_val = Memory::Read_U16(((0xF == m) ? addr : gecko_register[m]) & ~y);
		}
		else
		{
			// CT5 Part2 : 16bits Counter check
			// A-0ZZZZT MMMMXXXX

			left_val = (u16)(data) & ~(u16)(data >> 16);
			right_val = (u16)(addr >> 4);
		}

		switch (code.subtype & 0x3)
		{
			// CST0 : 16bits (endif, then) If equal
		case 0x0 :
			result = (left_val == right_val);
			break;

			// CST1 : 16bits (endif, then) If not equal
		case 0x1 :
			result = (left_val != right_val);
			break;

			// CST2 : 16bits (endif, then) If greater
		case 0x2 :
			result = (left_val > right_val);
			break;

			// CST3 : 16bits (endif, then) If lower
		case 0x3 :
			result = (left_val < right_val);
			break;
		}
	}	
	else if (code.subtype & 0x4)
	{
		// counters get reset if code execution is off
		code.address &= ~0x000FFFF0;
	}

	// if the conditional returned false, or it never ran because execution is off, increase the code execution counter
	if (false == result)
		++code_execution_counter;
	else if (code.subtype & 0x4)
	{
		// counters gets advanced if condition was true
		// right_val is the value of the counter

		code.address &= ~0x000FFFF0;
		code.address |= ((right_val+1) << 4);
	}

	return true;
}

// CT6 ASM Codes, On/Off switch and Address Range Check
// NOT COMPLETE, asm stuff not started
// TODO: Fix the ugliness
bool AsmSwitchRange()
{
	// the switch subtype modifies the code :/
	GeckoCode::Code& code = *current_code;

	// only run if code_execution is set or this code is a switch or rangecheck subtype
	// the switch and rangecheck run if execution_counter is 1 (directly inside the failed if) if they are an endif
	if (false == CodeExecution())
	{
		if (code.subtype < 0x6)
			return true;
		else if (1 != code_execution_counter)
			return true;
	}

	u32& data = code.data;

	switch (code.subtype)
	{
		// CST0 : Execute following ASM Code
	case 0x0 :
		// TODO:
		return false;	//
		break;

		// CST1 : Insert ASM code in the game
	case 0x1 :
		{
			const int number_of_codes = code.data;
			std::vector<u32>& asm_code = inserted_asm_codes[code.GetAddress()];
			if (asm_code.empty()) {
				for (int index = 0; index < number_of_codes; ++index) {
					// Reserved for padding
					if (current_code[index + 1].address != 0x60000000) {
						asm_code.push_back(current_code[index + 1].address);
					}

					// Reserved for b instruction
					if (current_code[index + 1].data != 0x00000000) {
						asm_code.push_back(current_code[index + 1].data);
					}
				}
			}

			// Though the next code starts at current_code+number_of_codes+1,
			// we add only number_of_codes. It is because the for statement in
			// RunGeckoCode() increments current_code.
			current_code += number_of_codes;

			break;
		}

		// CST3 : Create a branch
	case 0x3 :
		// watever
		//if (code.data)
		return false;	//
		break;

		// CST6 : On/Off switch
	case 0x6 :
		// in the 1st bit of code.data, i store if code execution was previously off
		// in the 2nd bit of code.data, i store the switch's on/off state
		if (CodeExecution())
		{
			if (data & 0x1)
				data ^= 0x2;	// if code exec was previously off, flip the switch

			// mark code execution as previously on
			data &= ~0x1;
		}
		else
		{
			// mark code execution as previously off
			data |= 0x1;
		}

		// set code execution to the state of the switch
		code_execution_counter = !(data & 0x2);
		break;

		// CST7 : Address range check (If... code)
	case 0x7 :
	{
		const bool is_endif = !!(code.address & 0x1);
		if (is_endif)
			--code_execution_counter;

		if (CodeExecution())
		{
			const u32 addr = (code.use_po ? pointer_address : base_address);
			if (addr < (data & 0xFFFF0000) || addr > (data << 16))
				++code_execution_counter;
		}
	}
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;
	}

	return true;
}

// CT7 End of codes, Endif (Else)
// COMPLETE, maybe
bool EndCodes()
{
	const GeckoCode::Code& code = *current_code;
	const u32& data = code.data;

	const u32 x = (data & 0xFFFF0000);
	const u32 y = (data << 16);

	// these 2 do not happen in the "CST7 : End of Code", but in that subtype they will be 0
	if (x)
		base_address = x;
	if (y)
		pointer_address = y;

	switch (code.subtype)
	{
		// CST0 : Full Terminator
	case 0x0 :
		// clears the code execution status
		code_execution_counter = 0;
		break;

		// CST1 : Endif (+else)
	case 0x1 :
	{
		// apply endifs
		const u8 v = (u8)code.address;
		if (code_execution_counter >= v)
			code_execution_counter -= v;
		else
		{
			// too many endifs
			// no it's not, i gotta fix my code execution on/off stuff
			code_execution_counter = 0;
		}

		const bool is_else = !!(code.address & 0x00100000);
		// apply else
		if (is_else)
			if (code_execution_counter <= 1)
				code_execution_counter ^= 1;
	}
		break;

		// CST7 : End of Code
	case 0x7 :
		// tell the code handler that there are no more codes in the code list
		// TODO: should this always stop all codes, even if execution is off?
		if (CodeExecution())
			code_execution_counter = -1;	// silly maybe
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;
	}

	return true;
}

}	// namespace Gecko
