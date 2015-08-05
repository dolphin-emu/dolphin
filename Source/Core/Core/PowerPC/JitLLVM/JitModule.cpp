// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitLLVM/JitHelpers.h"
#include "Core/PowerPC/JitLLVM/JitModule.h"

using namespace llvm;

static Type* GeneratePPCState()
{
	LLVMContext& con = getGlobalContext();

	// Basic types to make our life easier
	Type* llvm_i8 = Type::getInt8Ty(con);
	Type* llvm_i16 = Type::getInt16Ty(con);
	Type* llvm_i32 = Type::getInt32Ty(con);
	Type* llvm_i64 = Type::getInt64Ty(con);
	Type* llvm_f64 = Type::getDoubleTy(con);

	// This needs to match the ppcState in PowerPC.h otherwise we will be in trouble
	// XXX: This isn't a fully complete representation of the ppcState structure
	// Especially on x86_64 where the paired registers are required to be aligned to a boundary
	std::vector<Type*> struct_values =
	{
		ArrayType::get(llvm_i32, 32),           // GPRs
		llvm_i32,                               // PC
		llvm_i32,                               // NPC
		ArrayType::get(llvm_i64, 8),            // cr_val
		llvm_i32,                               // MSR
		llvm_i32,                               // fpscr
		llvm_i32,                               // Exceptions
		llvm_i32,                               // Downcount
		llvm_i8,                                // xer_ca
		llvm_i8,                                // xer_so_ov
		llvm_i16,                               // xer_stringctrl
		ArrayType::get(
			ArrayType::get(llvm_f64, 32), 2), // PS
		ArrayType::get(llvm_i32, 16),           // SR
		ArrayType::get(llvm_i32, 1024),         // SPRs
	};

	return StructType::create(struct_values, "PowerPCState", false);
}

LLVMModule::LLVMModule(ExecutionEngine* engine, u32 address)
	: m_engine(engine), m_address(address)
{
	LLVMContext& con = getGlobalContext();

	m_mod = new Module(StringFromFormat("ppc%08x", m_address), con);

	m_named_binder = new LLVMNamedBinding();
	m_binder = new LLVMBinding(m_engine, m_mod, m_named_binder);

	GenerateGlobalVariables();
	BindGlobalFunctions();

	FunctionType* main_type = GenerateFunctionType(Type::getVoidTy(con));
	m_func = new LLVMFunction(m_mod, m_address, main_type);

	GeneratePairedLoadFunction();
}

void LLVMModule::GenerateGlobalVariables()
{
	LLVMContext& con = getGlobalContext();

	std::vector<Constant*> dequantize_values;
	std::vector<Constant*> quantize_values;

	for (u32 i = 0; i < 32; ++i)
		dequantize_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1.0 / (1ULL << i)));
	for (u32 i = 32; i > 0; --i)
		dequantize_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1ULL << i));

	for (u32 i = 0; i < 32; ++i)
		quantize_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1ULL << i));
	for (u32 i = 32; i > 0; --i)
		quantize_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1.0 / (1ULL << i)));

	Constant* dequantize_constant = ConstantArray::get(ArrayType::get(Type::getFloatTy(con), 64), dequantize_values);
	Constant* quantize_constant = ConstantArray::get(ArrayType::get(Type::getFloatTy(con), 64), quantize_values);

	m_mod->getOrInsertGlobal("dequantize_table", ArrayType::get(Type::getFloatTy(con), 64));
	GlobalVariable* dequantize_global = m_mod->getNamedGlobal("dequantize_table");
	dequantize_global->setInitializer(dequantize_constant);

	m_mod->getOrInsertGlobal("quantize_table", ArrayType::get(Type::getFloatTy(con), 64));
	GlobalVariable* quantize_global = m_mod->getNamedGlobal("quantize_table");
	quantize_global->setInitializer(quantize_constant);

	/* Types */
	static Type* ppcStateType = GeneratePPCState();
	Type* ppcStatePtr = ppcStateType->getPointerTo();

	/* Constants */
	Constant* const_ppcstate_val = ConstantInt::getIntegerValue(ppcStatePtr, APInt(64, (uint64_t)&PowerPC::ppcState));
	m_mod->getOrInsertGlobal("ppcState_ptr", ppcStatePtr);
	GlobalVariable* ppcStatePtr_global = m_mod->getNamedGlobal("ppcState_ptr");
	ppcStatePtr_global->setInitializer(const_ppcstate_val);
}

void LLVMModule::BindGlobalFunctions()
{
	LLVMContext& con = getGlobalContext();

	Function* dispatcher_func;
	FunctionType* void_type = GenerateFunctionType(Type::getVoidTy(con));

	dispatcher_func = Function::Create(void_type, Function::ExternalLinkage, "Dispatcher", m_mod);
	dispatcher_func->setCallingConv(CallingConv::C);
	dispatcher_func->setDoesNotReturn();
	m_binder->Bind(dispatcher_func, (void*)JitInterface::GetDispatcher());

	// PowerPC::CheckExceptions
	{
		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::CheckExceptions", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)&PowerPC::CheckExceptions);
	}

	// Generate our loadstore calling functions
	// Read_U64
	{
		FunctionType* type = GenerateFunctionType(
			Type::getInt64Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Read_U64", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Read_U64);
	}
	// Read_U32
	{
		FunctionType* type = GenerateFunctionType(
			Type::getInt32Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Read_U32", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Read_U32);
	}
	// Read_U16
	{
		FunctionType* type = GenerateFunctionType(
			Type::getInt16Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Read_U16", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Read_U16);
	}
	// Read_U8
	{
		FunctionType* type = GenerateFunctionType(
			Type::getInt8Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Read_U8", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Read_U8);
	}
	// Write_U64
	{
		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con),
			Type::getInt64Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Write_U64", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Write_U64);
	}
	// Write_U32
	{
		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con),
			Type::getInt32Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Write_U32", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Write_U32);
	}
	// Write_U16
	{
		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con),
			Type::getInt16Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Write_U16", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Write_U16);
	}
	// Write_U8
	{
		FunctionType* type = GenerateFunctionType(
			Type::getVoidTy(con),
			Type::getInt8Ty(con),
			Type::getInt32Ty(con));

		Function* bound_func = Function::Create(type, Function::ExternalLinkage, "PowerPC::Write_U8", m_mod);
		bound_func->setCallingConv(CallingConv::C);
		m_binder->Bind(bound_func, (void*)PowerPC::Write_U8);
	}
}

void LLVMModule::GeneratePairedLoadFunction()
{
	LLVMContext& con = getGlobalContext();

	VectorType* ret_type = VectorType::get(Type::getFloatTy(con), 2);

	FunctionType* main_type = GenerateFunctionType(
		ret_type, // returns <2 x float>
		Type::getInt8Ty(con), // bool paired
		Type::getInt8Ty(con), // u3 type
		Type::getInt8Ty(con), // u6 scale
		Type::getInt32Ty(con)); // u32 address
	m_paired_load = new LLVMFunction(m_mod, "Helper_PairedLoad", main_type);

	BasicBlock* ending_block = m_paired_load->CreateBlock("EndBlock");

	// QUANTIZE_FLOAT
	BasicBlock* float_case      = m_paired_load->CreateBlock("Float_Single");
	BasicBlock* float_pair_case = m_paired_load->CreateBlock("Float_Paired");

	// QUANTIZE_U8
	BasicBlock* u8_case      = m_paired_load->CreateBlock("U8_Single");
	BasicBlock* u8_pair_case = m_paired_load->CreateBlock("U8_Paired");

	// QUANTIZE_U16
	BasicBlock* u16_case      = m_paired_load->CreateBlock("U16_Single");
	BasicBlock* u16_pair_case = m_paired_load->CreateBlock("U16_Paired");

	// QUANTIZE_S8
	BasicBlock* s8_case      = m_paired_load->CreateBlock("S8_Single");
	BasicBlock* s8_pair_case = m_paired_load->CreateBlock("S8_Paired");

	// QUANTIZE_S16
	BasicBlock* s16_case      = m_paired_load->CreateBlock("S16_Single");
	BasicBlock* s16_pair_case = m_paired_load->CreateBlock("S16_Paired");

	std::vector<Argument*> args;
	for (Argument& arg : m_paired_load->GetFunction()->args())
		args.push_back(&arg);

	IRBuilder<>* builder = m_paired_load->GetBuilder();

	std::vector<Constant*> default_ret_values;
		default_ret_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1.0f));
		default_ret_values.push_back(ConstantFP::get(Type::getFloatTy(con), 1.0f));

	Value* ret_values = ConstantVector::get(default_ret_values);

	std::vector<Constant*> invalid_values;
		invalid_values.push_back(ConstantFP::get(Type::getFloatTy(con), 0.0f));
		invalid_values.push_back(ConstantFP::get(Type::getFloatTy(con), 0.0f));

	Value* invalid_ret_values = ConstantVector::get(invalid_values);

	BasicBlock* start_block = &m_paired_load->GetFunction()->front();
	SwitchInst* switch_statement = builder->CreateSwitch(args[1], ending_block, 5);

	switch_statement->addCase(builder->getInt8(QUANTIZE_FLOAT), float_case);
	switch_statement->addCase(builder->getInt8(QUANTIZE_U8),    u8_case);
	switch_statement->addCase(builder->getInt8(QUANTIZE_U16),   u16_case);
	switch_statement->addCase(builder->getInt8(QUANTIZE_S8),    s8_case);
	switch_statement->addCase(builder->getInt8(QUANTIZE_S16),   s16_case);

	std::array<std::pair<Value*, Value*>, 5> phi_values;

	builder->SetInsertPoint(float_case);
	{
		Value* res0 = CreateReadU32(m_paired_load, args[3]);
		res0 = builder->CreateBitCast(res0, builder->getFloatTy());
		Value* final_res = builder->CreateInsertElement(ret_values, res0, builder->getInt32(0), "float_res");
		Value* cmp_val = builder->CreateICmpEQ(args[0], builder->getInt8(1));
		builder->CreateCondBr(cmp_val, float_pair_case, ending_block);

		builder->SetInsertPoint(float_pair_case);
			Value* res1 = CreateReadU32(m_paired_load, builder->CreateAdd(args[3], builder->getInt32(4)));
			res1 = builder->CreateBitCast(res1, builder->getFloatTy());
			Value* final_paired_res = builder->CreateInsertElement(final_res, res1, builder->getInt32(1));
			builder->CreateBr(ending_block);
		phi_values[0] = std::make_pair(final_res, final_paired_res);
	}

	builder->SetInsertPoint(u8_case);
	{
		Value* dequantize_value = LoadDequantizeValue(m_paired_load, args[2]);
		Value* res0 = CreateReadU8(m_paired_load, args[3], false);
		res0 = builder->CreateUIToFP(res0, builder->getFloatTy());
		res0 = builder->CreateFMul(res0, dequantize_value);
		Value* final_res = builder->CreateInsertElement(ret_values, res0, builder->getInt32(0), "u8_res");
		Value* cmp_val = builder->CreateICmpEQ(args[0], builder->getInt8(1));
		builder->CreateCondBr(cmp_val, u8_pair_case, ending_block);

		builder->SetInsertPoint(u8_pair_case);
			Value* res1 = CreateReadU8(m_paired_load, builder->CreateAdd(args[3], builder->getInt32(1)), false);
			res1 = builder->CreateUIToFP(res1, builder->getFloatTy());
			res1 = builder->CreateFMul(res1, dequantize_value);
			Value* final_paired_res = builder->CreateInsertElement(final_res, res1, builder->getInt32(1));
			builder->CreateBr(ending_block);
		phi_values[1] = std::make_pair(final_res, final_paired_res);
	}

	builder->SetInsertPoint(u16_case);
	{
		Value* dequantize_value = LoadDequantizeValue(m_paired_load, args[2]);
		Value* res0 = CreateReadU16(m_paired_load, args[3], false);
		res0 = builder->CreateUIToFP(res0, builder->getFloatTy());
		res0 = builder->CreateFMul(res0, dequantize_value);
		Value* final_res = builder->CreateInsertElement(ret_values, res0, builder->getInt32(0), "u16_res");
		Value* cmp_val = builder->CreateICmpEQ(args[0], builder->getInt8(1));
		builder->CreateCondBr(cmp_val, u16_pair_case, ending_block);

		builder->SetInsertPoint(u16_pair_case);
			Value* res1 = CreateReadU16(m_paired_load, builder->CreateAdd(args[3], builder->getInt32(2)), false);
			res1 = builder->CreateUIToFP(res1, builder->getFloatTy());
			res1 = builder->CreateFMul(res1, dequantize_value);
			Value* final_paired_res = builder->CreateInsertElement(final_res, res1, builder->getInt32(1));
			builder->CreateBr(ending_block);
		phi_values[2] = std::make_pair(final_res, final_paired_res);
	}

	builder->SetInsertPoint(s8_case);
	{
		Value* dequantize_value = LoadDequantizeValue(m_paired_load, args[2]);
		Value* res0 = CreateReadU8(m_paired_load, args[3], true);
		res0 = builder->CreateUIToFP(res0, builder->getFloatTy());
		res0 = builder->CreateFMul(res0, dequantize_value);
		Value* final_res = builder->CreateInsertElement(ret_values, res0, builder->getInt32(0), "s8_res");
		Value* cmp_val = builder->CreateICmpEQ(args[0], builder->getInt8(1));
		builder->CreateCondBr(cmp_val, s8_pair_case, ending_block);

		builder->SetInsertPoint(s8_pair_case);
			Value* res1 = CreateReadU8(m_paired_load, builder->CreateAdd(args[3], builder->getInt32(1)), true);
			res1 = builder->CreateSIToFP(res1, builder->getFloatTy());
			res1 = builder->CreateFMul(res1, dequantize_value);
			Value* final_paired_res = builder->CreateInsertElement(final_res, res1, builder->getInt32(1));
			builder->CreateBr(ending_block);
		phi_values[3] = std::make_pair(final_res, final_paired_res);
	}

	builder->SetInsertPoint(s16_case);
	{
		Value* dequantize_value = LoadDequantizeValue(m_paired_load, args[2]);
		Value* res0 = CreateReadU16(m_paired_load, args[3], true);
		res0 = builder->CreateUIToFP(res0, builder->getFloatTy());
		res0 = builder->CreateFMul(res0, dequantize_value);
		Value* final_res = builder->CreateInsertElement(ret_values, res0, builder->getInt32(0), "s16_res");
		Value* cmp_val = builder->CreateICmpEQ(args[0], builder->getInt8(1));
		builder->CreateCondBr(cmp_val, s16_pair_case, ending_block);

		builder->SetInsertPoint(s16_pair_case);
			Value* res1 = CreateReadU16(m_paired_load, builder->CreateAdd(args[3], builder->getInt32(2)), true);
			res1 = builder->CreateSIToFP(res1, builder->getFloatTy());
			res1 = builder->CreateFMul(res1, dequantize_value);
			Value* final_paired_res = builder->CreateInsertElement(final_res, res1, builder->getInt32(1));
			builder->CreateBr(ending_block);
		phi_values[4] = std::make_pair(final_res, final_paired_res);
	}

	builder->SetInsertPoint(ending_block);

	PHINode* final_phi = builder->CreatePHI(ret_type, 11);
	final_phi->addIncoming(phi_values[0].first, float_case);
	final_phi->addIncoming(phi_values[1].first, u8_case);
	final_phi->addIncoming(phi_values[2].first, u16_case);
	final_phi->addIncoming(phi_values[3].first, s8_case);
	final_phi->addIncoming(phi_values[4].first, s16_case);
	final_phi->addIncoming(phi_values[0].second, float_pair_case);
	final_phi->addIncoming(phi_values[1].second, u8_pair_case);
	final_phi->addIncoming(phi_values[2].second, u16_pair_case);
	final_phi->addIncoming(phi_values[3].second, s8_pair_case);
	final_phi->addIncoming(phi_values[4].second, s16_pair_case);
	final_phi->addIncoming(invalid_ret_values, start_block);
	builder->CreateRet(final_phi);
}

Value* LLVMModule::CreateReadU64(LLVMFunction* func, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Read_U64);

	return builder->CreateCall(bound_func, address);
}

Value* LLVMModule::CreateReadU32(LLVMFunction* func, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Read_U32);

	return builder->CreateCall(bound_func, address);
}

Value* LLVMModule::CreateReadU16(LLVMFunction* func, Value* address, bool needs_sext)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Read_U16);

	Value* res = builder->CreateCall(bound_func, address);

	if (needs_sext)
		return builder->CreateSExt(res, builder->getInt32Ty());
	else
		return builder->CreateZExt(res, builder->getInt32Ty());
}

Value* LLVMModule::CreateReadU8(LLVMFunction* func, Value* address, bool needs_sext)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Read_U8);

	Value* res = builder->CreateCall(bound_func, address);

	if (needs_sext)
		return builder->CreateSExt(res, builder->getInt32Ty());
	else
		return builder->CreateZExt(res, builder->getInt32Ty());
}

Value* LLVMModule::CreateWriteU64(LLVMFunction* func, Value* val, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Write_U64);

	std::vector<Value*> args = {val, address};

	return builder->CreateCall(bound_func, args);
}

Value* LLVMModule::CreateWriteU32(LLVMFunction* func, Value* val, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Write_U32);

	std::vector<Value*> args = {val, address};

	return builder->CreateCall(bound_func, args);
}

Value* LLVMModule::CreateWriteU16(LLVMFunction* func, Value* val, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Write_U16);

	val = builder->CreateTrunc(val, builder->getInt16Ty());

	std::vector<Value*> args = {val, address};

	return builder->CreateCall(bound_func, args);
}

Value* LLVMModule::CreateWriteU8(LLVMFunction* func, Value* val, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();
	Function* bound_func = m_binder->GetBinding((void*)PowerPC::Write_U8);

	val = builder->CreateTrunc(val, builder->getInt8Ty());

	std::vector<Value*> args = {val, address};

	return builder->CreateCall(bound_func, args);
}

Value* LLVMModule::LoadDequantizeValue(LLVMFunction* func, Value* scale)
{
	IRBuilder<>* builder = func->GetBuilder();

	GlobalVariable* dequantize_global = m_mod->getNamedGlobal("dequantize_table");
	const std::vector<Value*> loc =
	{
		builder->getInt32(0),
		scale,
	};

	Value* scale_val = builder->CreateGEP(dequantize_global, loc);
	return builder->CreateLoad(scale_val, "Dequantize_Scale");
}

Value* LLVMModule::CreatePairedLoad(LLVMFunction* func, Value* paired, Value* type, Value* scale, Value* address)
{
	IRBuilder<>* builder = func->GetBuilder();

	std::vector<Value*> args = {paired, type, scale, address};

	return builder->CreateCall(m_paired_load->GetFunction(), args);
}

