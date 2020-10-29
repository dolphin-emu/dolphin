// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

struct lua_State;
class IniFile;

namespace ScriptEngine
{
struct ScriptTarget
{
  std::string file_path;
  bool active = false;
};

// Top-level script instance definition and state.
class Script
{
public:
  // Shared class managing all resources held by an instance of a script.
  class Context
  {
  public:
    explicit Context(lua_State* lua_state) : L(lua_state) {}
    ~Context();
    void ExecuteHook(int lua_ref);

  public:
    Context* self = nullptr;

  private:
    lua_State* L;  // nil if failed to init
  };

public:
  explicit Script(const ScriptTarget&);
  ~Script();
  Script(const Script&) = delete;
  Script(Script&&) noexcept;
  [[nodiscard]] inline const std::string& file_path() const { return m_file_path; };

private:
  void Load();
  void Unload();

private:
  // Target state
  std::string m_file_path;

private:
  Context* m_ctx = nullptr;  // nullptr if script invalid
};

// External pointer to a Lua function, for use in Core/PowerPC.
// Must not be used after unloading the script.
class LuaFuncHandle
{
public:
  LuaFuncHandle(Script::Context* ctx, int lua_ref) : m_ctx(ctx), m_lua_ref(lua_ref) {}
  inline void Execute() { m_ctx->ExecuteHook(m_lua_ref); };
  [[nodiscard]] bool Equals(const LuaFuncHandle& other) const;
  [[nodiscard]] inline Script::Context* ctx() const { return m_ctx; };
  [[nodiscard]] inline int lua_ref() const { return m_lua_ref; };

private:
  Script::Context* m_ctx;
  int m_lua_ref;
};

inline bool operator==(const LuaFuncHandle& lhs, const LuaFuncHandle& rhs)
{
  return lhs.Equals(rhs);
}

void LoadScriptSection(const std::string& section, std::vector<ScriptTarget>& scripts,
                       IniFile& localIni);
void LoadScripts();
void Shutdown();

}  // namespace ScriptEngine
