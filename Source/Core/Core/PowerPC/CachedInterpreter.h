// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

class CachedInterpreter : public JitBase, JitBaseBlockCache
{
public:
	CachedInterpreter() : code_buffer(32000) {}
	~CachedInterpreter() {}

	void Init() override;
	void Shutdown() override;

	bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }

	void ClearCache() override;

	void Run() override;
	void SingleStep() override;

	void Jit(u32 address) override;

	JitBaseBlockCache* GetBlockCache() override { return this; }

	const char* GetName() override
	{
		return "Cached Interpreter";
	}

	void WriteLinkBlock(u8* location, const JitBlock& block) override;

	void WriteDestroyBlock(const u8* location, u32 address) override;

	const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; };

private:
	struct Instruction
	{
		typedef void (*CommonCallback)(UGeckoInstruction);
		typedef bool (*ConditionalCallback)(u32 data);

		Instruction() : type(INSTRUCTION_ABORT) {};
		Instruction(const CommonCallback c, UGeckoInstruction i) : common_callback(c), data(i.hex), type(INSTRUCTION_TYPE_COMMON) {};
		Instruction(const ConditionalCallback c, u32 d) : conditional_callback(c), data(d), type(INSTRUCTION_TYPE_CONDITIONAL) {};

		union
		{
			const CommonCallback common_callback;
			const ConditionalCallback conditional_callback;

		};
		u32 data;
		enum
		{
			INSTRUCTION_ABORT,
			INSTRUCTION_TYPE_COMMON,
			INSTRUCTION_TYPE_CONDITIONAL,
		} type;
	};

	const u8* GetCodePtr() { return (u8*)(m_code.data() + m_code.size()); }

	std::vector<Instruction> m_code;

	PPCAnalyst::CodeBuffer code_buffer;
};

