
#include "GeckoCode.h"

#include "Thread.h"
#include "HW/Memmap.h"

#include "vector"

namespace Gecko
{

enum
{
	// Code Types
	CODETYPE_WRITE_FILL =	0x0,
	CODETYPE_IF =			0x1,
	CODETYPE_BA_PO_OPS =	0x2,
	CODETYPE_FLOW_CONTROL =	0x3,
	CODETYPE_REGISTER_OPS =	0x4,
	CODETYPE_SPECIAL_IF =	0x5,
	CODETYPE_ASM_SWITCH_RANGE =	0x6,
	CODETYPE_END_CODES =	0x7,

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

static Common::CriticalSection active_codes_lock;

// currently running code
static GeckoCode::Code current_code;

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
	active_codes_lock.Enter();

	active_codes.clear();
	// add enabled codes
	std::vector<GeckoCode>::const_iterator
		gcodes_iter = gcodes.begin(),
		gcodes_end = gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
		if (gcodes_iter->enabled)
		{
			// TODO: apply modifiers
			// TODO: don't need description or creator string, just takin up memory
			active_codes.push_back(*gcodes_iter);
		}

	active_codes_lock.Leave();
}

bool RunGeckoCode(const GeckoCode& gecko_code)
{
	static bool (*code_type_funcs[])(void) =
	{ RamWriteAndFill, RegularIf, BaPoOps, FlowControl, RegisterOps, SpecialIf, AsmSwitchRange, EndCodes };

	base_address = 0x80000000;
	pointer_address = 0x80000000;
	code_execution_counter = 0;

	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = gecko_code.codes.begin(),
		codes_end = gecko_code.codes.end();
	for (; codes_iter != codes_end; ++codes_iter)
	{
		const GeckoCode::Code& code = *codes_iter;

		current_code = code;

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
			PanicAlert("GeckoCode failed to run (CT%i CST%i) (%s)"
				"(Either a bad code or the code type is not yet supported.)"
				, code.type, code.subtype, gecko_code.name.c_str());
			return false;
		}



	}


	return true;
}

bool RunActiveCodes()
{
	if (false == active_codes_lock.TryEnter())
		return true;

	std::vector<GeckoCode>::const_iterator
		gcodes_iter = active_codes.begin(),
		gcodes_end = active_codes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		RunGeckoCode(*gcodes_iter);
		// we don't need to stop all codes if one fails, maybe
		//if (false == RunGeckoCode(*gcodes_iter))
			//return false;
	}

	active_codes_lock.Leave();
	return true;
}

// CT0: Direct ram write/fill
// NOT COMPLETE, last 2 subtypes not started
bool RamWriteAndFill()
{
	const GeckoCode::Code& code = current_code;
	u32 new_addr = code.GetAddress();
	const u32& data = code.data;

	u16 count = (data >> 16) + 1;

	switch (code.subtype)
	{
		// CST0: 8bits Write & Fill
	case 0x0 :
		while (count--)
		{
			Memory::Write_U16((u16)data, new_addr);
			++new_addr;
		}
		break;

		// CST1: 16bits Write & Fill
	case 0x1 :
		while (count--)
		{
			Memory::Write_U16((u16)data, new_addr);
			new_addr += 2;
		}
		break;

		// CST2: 32bits Write
	case 0x2 :
		Memory::Write_U32((u32)data, new_addr);
		break;

		// CST3: String Code
	case 0x3 :
		// TODO:
		return false;
		break;

		// CST4: Serial Code
	case 0x4 :
	{
		// TODO: complete
		// u32 new_data = data;
		
		return false;
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
	const GeckoCode::Code& code = current_code;

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
	const GeckoCode::Code& code = current_code;

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
// NOT COMPLETE
bool FlowControl()
{
	const GeckoCode::Code& code = current_code;

	// only the return subtype runs when code execution is off
	if (false == CodeExecution() && code.subtype != 0x2)
		return true;

	// not all of these are used in all subtypes
	const u8 block_num = (u8)(code.data & 0xF);

	switch (code.subtype)
	{
		// CST0 : Set Repeat
	case 0x0 :
		// TODO: store address of next code as well
		block[block_num].address = code.address & 0xFFFF;
		// block[block_num].number = ;
		return false; //
		break;

		// CST1 : Execute Repeat
	case 0x1 :
		if (block[block_num].number)
		{
			--block[block_num].number;
			// TODO: jump to code block
		}
		return false;	//
		break;

		// CST2 : Return
	case 0x2 :
		if (((code.address >> 20) & 0xF) ^ (u32)CodeExecution())
			// TODO: jump to block[block_num].number
		return false;	//
		break;

		// CST3 : Goto
	case 0x3 :
		// TODO:
		return false;	//
		break;

		// CST4 : Gosub
	case 0x4 :
		// TODO:
		return false;	//
		break;

		// INVALID SUBTYPE
	default :
		return false;
		break;
	}

	return true;
}

// CT4 Gecko Register Operations
// NOT COMPLETE, need to do memory copy 1,2
bool RegisterOps()
{
	const GeckoCode::Code& code = current_code;

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
	case 0x5 :
		// TODO:
		return false;	//
		break;

		// CST6 : Memory Copy 2
	case 0x6 :
		// TODO:
		return false;	//
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
		// TODO: wuts this
		return false;
		break;

		// TODO: these float ops good?
		// A : fadds (single float add)
	case 0xA :
		*(float*)&ret = *(float*)&left + *(float*)&right;
		break;

		// B : fmuls (single float mul)
	case 0xB :
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
// NOT COMPLETE, part 2 (counter stuff) not started
bool SpecialIf()
{
	const GeckoCode::Code& code = current_code;

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
		const u32 addr = code.GetAddress() & ~0x1;
		const u32& data = code.data;

		// A-______ NM00YYYY

		if (code.subtype ^ 0x4)
		{
			// CT5 Part1 : Unknown values comparison

			const u8 n = (u8)(data >> 28);
			const u8 m = (u8)((data >> 24) & 0xF);
			const u16 y = (u16)data;

			// TODO: should these be signed? probably?
			const s16 left_val = Memory::Read_U16(((0xF == n) ? addr : gecko_register[n]) & ~y);
			const s16 right_val = Memory::Read_U16(((0xF == m) ? addr : gecko_register[m]) & ~y);

			switch (code.subtype)
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
		else
		{
			// CT5 Part2 : 16bits Counter check
			// TODO:

			switch (code.subtype)
			{
				// CST4 : 16bits (endif, then) If counter value equal
			case 0x4 :
				// TODO:
				return false;	//
				break;

				// CST5 : 16bits (endif, then) If counter value not equal
			case 0x5 :
				// TODO:
				return false;	//
				break;

				// CST6 : 16bits (endif, then) If counter value greater
			case 0x6 :
				// TODO:
				return false;	//
				break;

				// CST7 : 16bits (endif, then) If counter value lower
			case 0x7 :
				// TODO:
				return false;	//
				break;
			}
		}
	}

	// if the conditional returned false, or it never ran because execution is off, increase the code execution counter
	if (false == result)
		++code_execution_counter;

	return true;
}

// CT6 ASM Codes, On/Off switch and Address Range Check
// NOT COMPLETE, hardly started
// fix the logic flow in this one
bool AsmSwitchRange()
{
	const GeckoCode::Code& code = current_code;

	// only used for the last 2 subtypes
	const bool is_endif = !!(code.address & 0x1);

	// only run if code_execution is set or this code is a switch or rangecheck subtype
	// the switch and rangecheck run if exectution_counter is 1 (directly inside the failed if) if they are an endif
	if (false == CodeExecution())
	{
		if (code.subtype < 0x6)
			return true;
		else if (false == (1 == code_execution_counter && is_endif))
			return true;
	}

	const u32& data = code.data;

	switch (code.subtype)
	{
		// CST0 : Execute following ASM Code
	case 0x0 :
		// TODO:
		return false;	//
		break;

		// CST1 : Insert ASM code in the game
	case 0x1 :
		// TODO:
		return false;
		break;

		// CST3 : Create a branch
	case 0x3 :
		// TODO:
		return false;	//
		break;

		// CST6 : On/Off switch
	case 0x6 :
		// TODO:
		return false;	//
		break;

		// CST7 : Address range check (If... code)
	case 0x7 :
		if (code_execution_counter)
		{
			const u32 addr = (code.use_po ? pointer_address : base_address);
			if (addr >= (data & 0xFFFF0000) && addr <= (data << 16))
				--code_execution_counter;
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
	const GeckoCode::Code& code = current_code;
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
		// TODO: should this always stop all codes, even if execution is off?
		if (CodeExecution())
			code_execution_counter = -1;	// silly maybe
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
			return false;
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
