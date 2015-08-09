// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>

#include <llvm/InitializePasses.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "Common/GekkoDisassembler.h"

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/PPCAnalyst.h"

#include "Core/PowerPC/JitLLVM/Jit.h"
#include "Core/PowerPC/JitLLVM/JitFunction.h"
#include "Core/PowerPC/JitLLVM/JitHelpers.h"
#include "Core/PowerPC/JitLLVM/JitLLVM_Tables.h"

using namespace llvm;

void JitLLVM::Init()
{
	JitLLVMTables::InitTables();

	m_debug_enabled = false;

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;

	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);

	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	LLVMContext& con = getGlobalContext();

	m_named_binder = new LLVMNamedBinding();
	m_mem_manager = new LLVMMemoryManager(m_named_binder);
	m_main_mod = new Module("Dolphin LLVM JIT Recompiler", con);

	m_engine = EngineBuilder(std::unique_ptr<Module>(m_main_mod))
		.setEngineKind(EngineKind::JIT)
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(m_mem_manager))
		.create();
}

void JitLLVM::Shutdown()
{
	// The ExecutionEngine takes ownership of nearly everything
	// Delete the engine, delete the world.
	delete m_engine;
	delete m_named_binder;
}

void JitLLVM::DoDownCount()
{
	LLVMFunction* func = GetCurrentFunc();
	IRBuilder<>* builder = func->GetBuilder();

	Value* downcount = func->LoadDownCount();
	Value* res = builder->CreateSub(downcount, builder->getInt32(js.downcountAmount));
	func->StoreDownCount(res);
}

void JitLLVM::WriteExceptionExit(Value* val)
{
	LLVMFunction* func = GetCurrentFunc();

	IRBuilder<>* builder = func->GetBuilder();

	DoDownCount();

	func->StorePC(val);
	func->StoreNPC(val);

	Function* check_exceptions_func = m_cur_binder->GetBinding((void*)&PowerPC::CheckExceptions);
	builder->CreateCall(check_exceptions_func);

	func->StorePC(func->LoadNPC());

	Function* dispatcher_func = m_cur_binder->GetBinding(m_cur_binder->GetBinding("Dispatcher"));
	builder->CreateCall(dispatcher_func);
	builder->CreateRetVoid();
}

void JitLLVM::WriteExit(u32 destination)
{
	LLVMFunction* func = GetCurrentFunc();

	IRBuilder<>* builder = GetCurrentFunc()->GetBuilder();

	DoDownCount();

	func->StorePC(builder->getInt32(destination));

	Function* dispatcher_func = m_cur_binder->GetBinding(m_cur_binder->GetBinding("Dispatcher"));
	builder->CreateCall(dispatcher_func);
	builder->CreateRetVoid();
}

void JitLLVM::WriteExitInValue(Value* destination)
{
	LLVMFunction* func = GetCurrentFunc();

	IRBuilder<>* builder = GetCurrentFunc()->GetBuilder();

	DoDownCount();

	func->StorePC(destination);

	Function* dispatcher_func = m_cur_binder->GetBinding(m_cur_binder->GetBinding("Dispatcher"));
	builder->CreateCall(dispatcher_func);
	builder->CreateRetVoid();
}

void JitLLVM::FallBackToInterpreter(LLVMFunction* func, UGeckoInstruction inst)
{
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);

	LLVMContext& con = getGlobalContext();

	IRBuilder<>* builder = func->GetBuilder();

	Function* bound_func = m_cur_binder->GetBinding((void*)instr);

	if (!bound_func)
	{
		// Haven't ever bound this interpreter function
		// Generate it and then bind it
		GekkoOPInfo* opcodeInfo = GetOpInfo(inst);

		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con),
			Type::getInt32Ty(con));

		bound_func = Function::Create(type, Function::ExternalLinkage, opcodeInfo->opname, m_cur_mod->GetModule());
		m_cur_binder->Bind(bound_func, (void*)instr);
	}

	builder->CreateCall(bound_func, builder->getInt32(inst.hex));
}

void JitLLVM::Break(LLVMFunction* func, UGeckoInstruction inst)
{
	// XXX: Actually break!
}

void JitLLVM::DoNothing(LLVMFunction* func, UGeckoInstruction inst)
{
}

void JitLLVM::Jit(u32 em_address)
{
	if (em_address == 0)
	{
		Core::SetState(Core::CORE_PAUSE);
		WARN_LOG(DYNA_REC, "ERROR: Compiling at 0. LR=%08x CTR=%08x", LR, CTR);
	}

	js.isLastInstruction = false;
	js.firstFPInstructionFound = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.downcountAmount = 0;
	js.skipInstructions = 0;

	int blockSize = code_buffer.GetSize();

	if (SConfig::GetInstance().bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		blockSize = 1;
	}

	u32 nextPC = em_address;
	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	nextPC = analyzer.Analyze(em_address, &code_block, &code_buffer, blockSize);

	legacy::PassManager PM;
	PassManagerBuilder Builder;
	Builder.OptLevel = 2;
	Builder.populateModulePassManager(PM);
	PM.add(createSLPVectorizerPass());
	raw_ostream& out = outs();

	if (m_debug_enabled)
		PM.add(createPrintModulePass(out));

	LLVMModule* mod = new LLVMModule(m_engine, em_address);
	m_mods[em_address] = mod;

	m_cur_mod = mod;
	m_cur_func = mod->GetFunction();
	m_cur_binder = mod->GetBinding();
	m_mem_manager->SetNamedBinder(mod->GetNamedBinding());

	// Build our GEP references first
	m_cur_func->BuildGEPs();

	PPCAnalyst::CodeOp *ops = code_buffer.codebuffer;

	if (m_debug_enabled)
	{
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			std::ostringstream ppc_disasm;
			const PPCAnalyst::CodeOp &op = ops[i];
			std::string opcode = GekkoDisassembler::Disassemble(op.inst.hex, op.address);
			ppc_disasm << std::setfill('0') << std::setw(8) << std::hex << op.address;
			ppc_disasm << " " << opcode << std::endl;
			printf("%s", ppc_disasm.str().c_str());
		}
	}

	IRBuilder<>* builder = m_cur_func->GetBuilder();

	for (u32 i = 0; i < code_block.m_num_instructions; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		const GekkoOPInfo *opinfo = ops[i].opinfo;
		js.downcountAmount += opinfo->numCycles;

		if (i == (code_block.m_num_instructions - 1))
		{
			// WARNING - cmp->branch merging will screw this up.
			js.isLastInstruction = true;
		}

		if (!ops[i].skip)
		{
			if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
			{
				BasicBlock* disabled_block = m_cur_func->CreateBlock("fp_disabled");
				BasicBlock* enabled_block = m_cur_func->CreateBlock("fp_enabled");

				Value* msr = m_cur_func->LoadMSR();
				Value* fp_enabled = builder->CreateAnd(msr, builder->getInt32(1 << 13));
				fp_enabled = builder->CreateICmpNE(fp_enabled, builder->getInt32(0));

				builder->CreateCondBr(fp_enabled, enabled_block, disabled_block);

				builder->SetInsertPoint(disabled_block);
					Value* exceptions = m_cur_func->LoadExceptions();
					exceptions = builder->CreateOr(exceptions, builder->getInt32(EXCEPTION_FPU_UNAVAILABLE));
					m_cur_func->StoreExceptions(exceptions);
					WriteExceptionExit(builder->getInt32(js.compilerPC));

				builder->SetInsertPoint(enabled_block);
					// Continue on with our life

				js.firstFPInstructionFound = true;
			}

			JitLLVMTables::CompileInstruction(this, m_cur_func, ops[i]);
		}

		i += js.skipInstructions;
		js.skipInstructions = 0;
	}

	if (code_block.m_broken)
		WriteExit(nextPC);

	verifyModule(*m_cur_mod->GetModule(), &out);
	PM.run(*m_cur_mod->GetModule());

	m_engine->addModule(std::unique_ptr<Module>(mod->GetModule()));
	m_engine->finalizeObject();

	//std::string loc = StringFromFormat("ppc%08x", em_address);
	//void* normal_entry_ptr = (void*)m_engine->getFunctionAddress(loc.c_str());
	// XXX: Enable when we actually start giving blocks to swap in
	//b->normalEntry = normal_entry_ptr;
	// XXX: Enable once we enable block linking
	//b->checkedEntry = start;
}

