// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Hooks.h"

#include <tuple>

// Generate the lists of argument names
#define NAMES_STRING(TYPE, NAME) #NAME
#define ARGUMENT_NAME_STRINGS(NAME, ...)                                                           \
  static std::array<std::string, std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value>   \
      NAME##_argnames = {__VA_ARGS__};
HOOKABLE_EVENT_TABLE(ARGUMENT_NAME_STRINGS, NAMES_STRING)

namespace Hook
{
// Generate the Definition of all the hooks
#define TYPES_ONLY(TYPE, NAME) TYPE
#define DEFINITION(NAME, ...) HookableEvent<__VA_ARGS__> NAME(NAME##_argnames);
HOOKABLE_EVENT_TABLE(DEFINITION, TYPES_ONLY)
}

#if defined(_WIN32)
#define LIBDOLPHIN_EXPORT __declspec(dllexport)
#else
#define LIBDOLPHIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
// Generate RegisterHook_HookName(const char* name, function_pointer) c functions that we can export
#define DLL_EXPORT_REGISTER(NAME, ...)                                                             \
  LIBDOLPHIN_EXPORT void RegisterHook_##NAME(const char* name, void (*callback)())                 \
  {                                                                                                \
    auto casted_callback = reinterpret_cast<decltype(Hook::NAME)::CallbackType>(callback);         \
    Hook::NAME.RegisterCallback(name, casted_callback);                                            \
  }
HOOKABLE_EVENT_TABLE(DLL_EXPORT_REGISTER, TYPES_ONLY)

// Generate DeleteHook_HookName(const char* name, function_pointer) c functions that we can export
#define DLL_EXPORT_DELETE(NAME, ...)                                                               \
  LIBDOLPHIN_EXPORT void DeleteHook_##NAME(const char* name) { Hook::NAME.DeleteCallback(name); }
HOOKABLE_EVENT_TABLE(DLL_EXPORT_DELETE, TYPES_ONLY)
}