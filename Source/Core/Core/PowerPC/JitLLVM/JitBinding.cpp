// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/MemoryUtil.h"
#include "Core/PowerPC/JitLLVM/JitBinding.h"

using namespace llvm;

void* LLVMNamedBinding::GetBinding(const std::string& name)
{
	auto binding = m_named_map.find(name);
	if (binding == m_named_map.end())
		return nullptr;
	return binding->second;
}

void LLVMNamedBinding::Bind(const std::string& Name, void* address)
{
	m_named_map[Name] = address;
}

Function* LLVMBinding::GetBinding(void* address)
{
	auto binding = m_map.find(address);
	if (binding == m_map.end())
		return nullptr;
	return binding->second;
}

void LLVMBinding::Bind(llvm::Function* stub, void* address)
{
	if (!m_engine->getPointerToGlobalIfAvailable(stub))
		m_engine->addGlobalMapping(stub, address);

	m_map[address] = stub;
	m_named_binding->Bind(stub->getName(), address);
}

// Memory Management
LLVMMemoryManager::~LLVMMemoryManager()
{
	for (auto& it : m_code_sections)
		FreeMemoryPages(it.second.first, it.second.second);

	for (auto& it : m_data_sections)
		delete[] it.second;

	m_code_sections.clear();
	m_data_sections.clear();
}

u8* LLVMMemoryManager::allocateCodeSection(
	uintptr_t Size, unsigned Alignment, unsigned SectionID,
	StringRef SectionName)
{
	// XXX: Respect alignment
	u8* section = (u8*)AllocateExecutableMemory(Size);
	m_code_sections[SectionID] = std::make_pair(section, Size);
	return section;
}

u8* LLVMMemoryManager::allocateDataSection(
	uintptr_t Size, unsigned Alignment, unsigned SectionID,
	StringRef SectionName, bool IsReadOnly)
{
	// XXX: Respect alignment
	u8* section = new u8(Size);
	m_data_sections[SectionID] = section;
	return section;
}

bool LLVMMemoryManager::finalizeMemory(std::string* ErrMsg)
{
	return true;
}

