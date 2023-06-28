#include "DynamicLibrary.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::DynamicLibrary(const char* filename)
{
  Open(filename);
}

DynamicLibrary::DynamicLibrary(void* handle)
{
  m_handle = handle;
}

DynamicLibrary::~DynamicLibrary()
{
  Close();
}

DynamicLibrary& DynamicLibrary::operator=(void* handle)
{
  m_handle = handle;
  return *this;
}

std::string DynamicLibrary::GetUnprefixedFilename(const char* filename)
{
#if defined(_WIN32)
  return std::string(filename) + ".dll";
#elif defined(__APPLE__)
  return std::string(filename) + ".dylib";
#else
  return std::string(filename) + ".so";
#endif
}


bool DynamicLibrary::Open(const char* filename)
{
#ifdef _WIN32
  m_handle = reinterpret_cast<void*>(LoadLibraryA(filename));
#else
  m_handle = dlopen(filename, RTLD_NOW);
#endif
  return m_handle != nullptr;
}

void DynamicLibrary::Close()
{
  if (!IsOpen())
    return;

#ifdef _WIN32
  FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
  dlclose(m_handle);
#endif
  m_handle = nullptr;
}

void* DynamicLibrary::GetSymbolAddress(const char* name) const
{
#ifdef _WIN32
  return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
#else
  return reinterpret_cast<void*>(dlsym(m_handle, name));
#endif
}
