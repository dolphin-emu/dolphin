#ifndef DYNAMIC_LIBRARY_H
#define DYNAMIC_LIBRARY_H

#include <string>

/**
 * Provides a platform-independent interface for loading a dynamic library and retrieving symbols.
 * The interface maintains an internal reference count to allow one handle to be shared between
 * multiple users.
 */
class DynamicLibrary final
{
public:
  // Default constructor, does not load a library.
  DynamicLibrary();

  // Automatically loads the specified library. Call IsOpen() to check validity before use.
  DynamicLibrary(const char* filename);

  DynamicLibrary(void* handle);

  // Closes the library.
  ~DynamicLibrary();

  DynamicLibrary(const DynamicLibrary&) = delete;
  DynamicLibrary(DynamicLibrary&&) = delete;

  DynamicLibrary& operator=(const DynamicLibrary&) = delete;
  DynamicLibrary& operator=(DynamicLibrary&&) = delete;

  DynamicLibrary& operator=(void*);

  // Returns the specified library name with the platform-specific suffix added.
  static std::string GetUnprefixedFilename(const char* filename);

  // Returns true if a module is loaded, otherwise false.
  bool IsOpen() const { return m_handle != nullptr; }

  // Loads (or replaces) the handle with the specified library file name.
  // Returns true if the library was loaded and can be used.
  bool Open(const char* filename);

  // Unloads the library, any function pointers from this library are no longer valid.
  void Close();

  // Returns the address of the specified symbol (function or variable) as an untyped pointer.
  // If the specified symbol does not exist in this library, nullptr is returned.
  void* GetSymbolAddress(const char* name) const;

  // Obtains the address of the specified symbol, automatically casting to the correct type.
  // Returns true if the symbol was found and assigned, otherwise false.
  template <typename T>
  bool GetSymbol(const char* name, T* ptr) const
  {
    *ptr = reinterpret_cast<T>(GetSymbolAddress(name));
    return *ptr != nullptr;
  }

private:
  // Platform-dependent data type representing a dynamic library handle.
  void* m_handle = nullptr;
};

#endif
