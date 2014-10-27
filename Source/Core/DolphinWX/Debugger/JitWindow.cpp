// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <disasm.h>        // Bochs
#include <sstream>

#if defined(HAS_LLVM)
// PowerPC.h defines PC.
// This conflicts with a function that has an argument named PC
#undef PC
#include <llvm-c/Disassembler.h>
#include <llvm-c/Target.h>
#endif

#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/listbase.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/window.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/JitWindow.h"

#if defined(HAS_LLVM)
// This class declaration should be in the header
// Due to the conflict with the PC define and the function with PC as an argument
// it has to be in this file instead.
// Once that conflict is resolved this can be moved to the header
class HostDisassemblerLLVM : public HostDisassembler
{
public:
	HostDisassemblerLLVM(const std::string host_disasm);
	~HostDisassemblerLLVM()
	{
		if (m_can_disasm)
			LLVMDisasmDispose(m_llvm_context);
	}

private:
	bool m_can_disasm;
	LLVMDisasmContextRef m_llvm_context;

	std::string DisassembleHostBlock(const u8* code_start, const u32 code_size, u32* host_instructions_count) override;
};

HostDisassemblerLLVM::HostDisassemblerLLVM(const std::string host_disasm)
	: m_can_disasm(false)
{
	LLVMInitializeAllTargetInfos();
	LLVMInitializeAllTargetMCs();
	LLVMInitializeAllDisassemblers();

	m_llvm_context = LLVMCreateDisasm(host_disasm.c_str(), nullptr, 0, 0, nullptr);

	// Couldn't create llvm context
	if (!m_llvm_context)
		return;
	LLVMSetDisasmOptions(m_llvm_context,
		LLVMDisassembler_Option_AsmPrinterVariant |
		LLVMDisassembler_Option_PrintLatency);

	m_can_disasm = true;
}

std::string HostDisassemblerLLVM::DisassembleHostBlock(const u8* code_start, const u32 code_size, u32 *host_instructions_count)
{
	if (!m_can_disasm)
		return "(No LLVM context)";

	u64 disasmPtr = (u64)code_start;
	const u8 *end = code_start + code_size;

	std::ostringstream x86_disasm;
	while ((u8*)disasmPtr < end)
	{
		char inst_disasm[256];
		disasmPtr += LLVMDisasmInstruction(m_llvm_context, (u8*)disasmPtr, (u64)(end - disasmPtr), (u64)disasmPtr, inst_disasm, 256);
		x86_disasm << inst_disasm << std::endl;
		(*host_instructions_count)++;
	}

	return x86_disasm.str();
}
#endif

std::string HostDisassembler::DisassembleBlock(u32* address, u32* host_instructions_count, u32* code_size)
{
	if (!jit)
	{
		*host_instructions_count = 0;
		*code_size = 0;
		return "(No JIT active)";
	}

	int block_num = jit->GetBlockCache()->GetBlockNumberFromStartAddress(*address);
	if (block_num < 0)
	{
		for (int i = 0; i < 500; i++)
		{
			block_num = jit->GetBlockCache()->GetBlockNumberFromStartAddress(*address - 4 * i);
			if (block_num >= 0)
				break;
		}

		if (block_num >= 0)
		{
			JitBlock* block = jit->GetBlockCache()->GetBlock(block_num);
			if (!(block->originalAddress <= *address &&
			    block->originalSize + block->originalAddress >= *address))
				block_num = -1;
		}

		// Do not merge this "if" with the above - block_num changes inside it.
		if (block_num < 0)
		{
			host_instructions_count = 0;
			code_size = 0;
			return "(No translation)";
		}
	}

	JitBlock* block = jit->GetBlockCache()->GetBlock(block_num);

	const u8* code = (const u8*)jit->GetBlockCache()->GetCompiledCodeFromBlock(block_num);

	*code_size = block->codeSize;
	*address = block->originalAddress;
	return DisassembleHostBlock(code, block->codeSize, host_instructions_count);
}

HostDisassemblerX86::HostDisassemblerX86()
{
	m_disasm.set_syntax_intel();
}

std::string HostDisassemblerX86::DisassembleHostBlock(const u8* code_start, const u32 code_size, u32* host_instructions_count)
{
	u64 disasmPtr = (u64)code_start;
	const u8* end = code_start + code_size;

	std::ostringstream x86_disasm;
	while ((u8*)disasmPtr < end)
	{
		char inst_disasm[256];
		disasmPtr += m_disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, inst_disasm);
		x86_disasm << inst_disasm << std::endl;
		(*host_instructions_count)++;
	}

	return x86_disasm.str();
}

enum
{
	IDM_REFRESH_LIST = 23350,
	IDM_PPC_BOX,
	IDM_X86_BOX,
	IDM_NEXT,
	IDM_PREV,
	IDM_BLOCKLIST,
};

BEGIN_EVENT_TABLE(CJitWindow, wxPanel)
	//EVT_TEXT(IDM_ADDRBOX, CJitWindow::OnAddrBoxChange)
	//EVT_LISTBOX(IDM_SYMBOLLIST, CJitWindow::OnSymbolListChange)
	//EVT_HOST_COMMAND(wxID_ANY, CJitWindow::OnHostMessage)
	EVT_BUTTON(IDM_REFRESH_LIST, CJitWindow::OnRefresh)
END_EVENT_TABLE()

CJitWindow::CJitWindow(wxWindow* parent, wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style, const wxString& name)
: wxPanel(parent, id, pos, size, style, name)
{
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerSplit = new wxBoxSizer(wxHORIZONTAL);
	sizerSplit->Add(ppc_box = new wxTextCtrl(this, IDM_PPC_BOX, "(ppc)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerSplit->Add(x86_box = new wxTextCtrl(this, IDM_X86_BOX, "(x86)",
				wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerBig->Add(block_list = new JitBlockList(this, IDM_BLOCKLIST,
				wxDefaultPosition, wxSize(100, 140),
				wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING),
				0, wxEXPAND);
	sizerBig->Add(sizerSplit, 2, wxEXPAND);
	// sizerBig->Add(memview, 5, wxEXPAND);
	// sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerBig->Add(button_refresh = new wxButton(this, IDM_REFRESH_LIST, _("&Refresh")));
	// sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, ""));
	// sizerRight->Add(new wxButton(this, IDM_SETPC, _("S&et PC")));

	SetSizer(sizerBig);

	sizerSplit->Fit(this);
	sizerBig->Fit(this);

#if defined(_M_X86) && defined(HAS_LLVM)
	m_disassembler.reset(new HostDisassemblerLLVM("x86_64-none-unknown"));
#elif defined(_M_X86)
	m_disassembler.reset(new HostDisassemblerX86());
#elif defined(_M_ARM_64) && defined(HAS_LLVM)
	m_disassembler.reset(new HostDisassemblerLLVM("aarch64-none-unknown"));
#elif defined(_M_ARM_32) && defined(HAS_LLVM)
	m_disassembler.reset(new HostDisassemblerLLVM("armv7-none-unknown"));
#else
	m_disassembler.reset(new HostDisassembler());
#endif
}

void CJitWindow::OnRefresh(wxCommandEvent& /*event*/)
{
	block_list->Update();
}

void CJitWindow::ViewAddr(u32 em_address)
{
	Show(true);
	Compare(em_address);
	SetFocus();
}

void CJitWindow::Compare(u32 em_address)
{
	// Get host side code disassembly
	u32 host_instructions_count = 0;
	u32 host_code_size = 0;
	std::string host_instructions_disasm;
	host_instructions_disasm = m_disassembler->DisassembleBlock(&em_address, &host_instructions_count, &host_code_size);

	x86_box->SetValue(host_instructions_disasm);

	// == Fill in ppc box
	u32 ppc_addr = em_address;
	PPCAnalyst::CodeBuffer code_buffer(32000);
	PPCAnalyst::BlockStats st;
	PPCAnalyst::BlockRegStats gpa;
	PPCAnalyst::BlockRegStats fpa;
	PPCAnalyst::CodeBlock code_block;
	PPCAnalyst::PPCAnalyzer analyzer;
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);

	code_block.m_stats = &st;
	code_block.m_gpa = &gpa;
	code_block.m_fpa = &fpa;

	if (analyzer.Analyze(ppc_addr, &code_block, &code_buffer, 32000) != 0xFFFFFFFF)
	{
		std::ostringstream ppc_disasm;
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			const PPCAnalyst::CodeOp &op = code_buffer.codebuffer[i];
			std::string opcode = GekkoDisassembler::Disassemble(op.inst.hex, op.address);
			ppc_disasm << std::setfill('0') << std::setw(8) << std::hex << op.address;
			ppc_disasm << " " << opcode << std::endl;
		}

		// Add stats to the end of the ppc box since it's generally the shortest.
		ppc_disasm << std::dec << std::endl;

		// Add some generic analysis
		if (st.isFirstBlockOfFunction)
			ppc_disasm << "(first block of function)" << std::endl;
		if (st.isLastBlockOfFunction)
			ppc_disasm << "(last block of function)" << std::endl;

		ppc_disasm << st.numCycles << " estimated cycles" << std::endl;

		ppc_disasm << "Num instr: PPC: " << code_block.m_num_instructions
		           << " x86: " << host_instructions_count
		           << " (blowup: " << 100 * host_instructions_count / code_block.m_num_instructions - 100
		           << "%)" << std::endl;

		ppc_disasm << "Num bytes: PPC: " << code_block.m_num_instructions * 4
		           << " x86: " << host_code_size
		           << " (blowup: " << 100 * host_code_size / (4 * code_block.m_num_instructions) - 100
		           << "%)" << std::endl;

		ppc_box->SetValue(ppc_disasm.str());
	}
	else
	{
		ppc_box->SetValue(StringFromFormat("(non-code address: %08x)", em_address));
		x86_box->SetValue("---");
	}
}

void CJitWindow::Update()
{

}

void CJitWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_NOTIFYMAPLOADED:
			//NotifyMapLoaded();
			break;
	}
}

// JitBlockList
//================

enum
{
	COLUMN_ADDRESS,
	COLUMN_PPCSIZE,
	COLUMN_X86SIZE,
	COLUMN_NAME,
	COLUMN_FLAGS,
	COLUMN_NUMEXEC,
	COLUMN_COST,  // (estimated as x86size * numexec)
};

JitBlockList::JitBlockList(wxWindow* parent, const wxWindowID id,
		const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style) // | wxLC_VIRTUAL)
{
	Init();
}

void JitBlockList::Init()
{
	InsertColumn(COLUMN_ADDRESS, _("Address"));
	InsertColumn(COLUMN_PPCSIZE, _("PPC Size"));
	InsertColumn(COLUMN_X86SIZE, _("x86 Size"));
	InsertColumn(COLUMN_NAME, _("Symbol"));
	InsertColumn(COLUMN_FLAGS, _("Flags"));
	InsertColumn(COLUMN_NUMEXEC, _("NumExec"));
	InsertColumn(COLUMN_COST, _("Cost"));
}

void JitBlockList::Update()
{
}
