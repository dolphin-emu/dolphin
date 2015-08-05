// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitLLVM/Jit.h"

template<typename... Args>
llvm::FunctionType* GenerateFunctionType(llvm::Type* ret_type, Args... args)
{
	std::vector<llvm::Type*> types = {args...};
	return llvm::FunctionType::get(ret_type, types, false);
}

