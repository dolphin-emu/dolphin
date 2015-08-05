// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>

#include "Core/PowerPC/JitLLVM/Jit.h"

class LLVMNamedBinding
{
private:
	std::unordered_map<std::string, void*> m_named_map;

public:
	void* GetBinding(const std::string& name);
	void Bind(const std::string& Name, void* address);
};

class LLVMBinding
{
private:
	llvm::ExecutionEngine* m_engine;
	llvm::Module* m_mod;
	LLVMNamedBinding* m_named_binding;

	std::unordered_map<void*, llvm::Function*> m_map;

public:
	LLVMBinding(llvm::ExecutionEngine* engine, llvm::Module* mod, LLVMNamedBinding* named_binder)
		: m_engine(engine), m_mod(mod), m_named_binding(named_binder)
	{}

	llvm::Function* GetBinding(void* address);
	void Bind(llvm::Function* stub, void* address);

	// Named bindings
	void* GetBinding(const std::string& name) { return m_named_binding->GetBinding(name); }
	void Bind(const std::string& Name, void* address);
};

// This is a super annoying class that needs to be implemented to get custom symbol resolving
class LLVMMemoryManager : public llvm::RTDyldMemoryManager
{
private:
	LLVMNamedBinding* m_binder;

	std::unordered_map<unsigned, std::pair<uint8_t*, uint64_t>> m_code_sections;
	std::unordered_map<unsigned, uint8_t*> m_data_sections;

public:
	LLVMMemoryManager(LLVMNamedBinding* binder)
		: m_binder(binder)
	{}
	~LLVMMemoryManager();

	void SetNamedBinder(LLVMNamedBinding* binder)
	{
		m_binder = binder;
	}

	/// This method returns the address of the specified function or variable.
	/// It is used to resolve symbols during module linking.
	uint64_t getSymbolAddress(const std::string &Name) override
	{
		void* our_bind = m_binder->GetBinding(Name);
		if (our_bind)
			return (uint64_t)our_bind;
		else
			return llvm::RTDyldMemoryManager::getSymbolAddressInProcess(Name);
	}

	/// Allocate a memory block of (at least) the given size suitable for
	/// executable code. The SectionID is a unique identifier assigned by the JIT
	/// engine, and optionally recorded by the memory manager to access a loaded
	/// section.
	uint8_t *allocateCodeSection(
		uintptr_t Size, unsigned Alignment, unsigned SectionID,
		llvm::StringRef SectionName) override;

	/// Allocate a memory block of (at least) the given size suitable for data.
	/// The SectionID is a unique identifier assigned by the JIT engine, and
	/// optionally recorded by the memory manager to access a loaded section.
	uint8_t *allocateDataSection(
		uintptr_t Size, unsigned Alignment, unsigned SectionID,
		llvm::StringRef SectionName, bool IsReadOnly) override;


	/// This method is called when object loading is complete and section page
	/// permissions can be applied.  It is up to the memory manager implementation
	/// to decide whether or not to act on this method.  The memory manager will
	/// typically allocate all sections as read-write and then apply specific
	/// permissions when this method is called.  Code sections cannot be executed
	/// until this function has been called.  In addition, any cache coherency
	/// operations needed to reliably use the memory are also performed.
	///
	/// Returns true if an error occurred, false otherwise.
	bool finalizeMemory(std::string *ErrMsg = nullptr) override;
};
