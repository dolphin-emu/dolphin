#pragma once
#include "fmt/format.h"
#include "lua.hpp"
#include <memory>
#include <utility>
#include <functional>
#include <fstream>

#include "Core/Scripting/HelperClasses/GCButtons.h"

#include "Core/Scripting/HelperClasses/ClassMetadata.h"

#include "Core/Scripting/HelperClasses/ClassFunctionsResolver.h"
#include "Common/FileUtil.h"

namespace Scripting::Lua
{

extern std::function<void(const std::string&)>* print_callback;
extern bool set_print_callback;
extern bool set_script_end_callback;
extern std::function<void(int)>* script_end_callback;
const char* THIS_VARIABLE_NAME = "THIS__Internal984Z234"; //Making this something unlikely to overlap with a user-defined global.
int x = 0;

class LuaScriptContext : public ScriptContext
{
public:

  lua_State* main_lua_thread;
  lua_State* frame_callback_lua_thread;
  lua_State* instruction_address_hit_callback_lua_thread;
  lua_State* memory_address_read_from_callback_lua_thread;
  lua_State* memory_address_written_to_callback_lua_thread;
  lua_State* gc_controller_input_polled_callback_lua_thread;
  lua_State* wii_input_polled_callback_lua_thread;

  lua_State* current_lua_state_thread;

  std::vector<int> frame_callback_locations;
  std::vector<int> gc_controller_input_polled_callback_locations;
  std::vector<int> wii_controller_input_polled_callback_locations;

  int index_of_next_frame_callback_to_execute;

  std::unordered_map<size_t, std::vector<int>> map_of_instruction_address_to_lua_callback_locations;

  std::unordered_map<size_t, std::vector<int>>
      map_of_memory_address_read_from_to_lua_callback_locations;

  std::unordered_map<size_t, std::vector<int>>
      map_of_memory_address_written_to_to_lua_callback_locations;

  static int CustomPrintFunction(lua_State* lua_state)
  {
    int nargs = lua_gettop(lua_state);
    std::string output_string;
    for (int i = 1; i <= nargs; i++)
    {
      if (lua_isstring(lua_state, i))
      {
        output_string.append(lua_tostring(lua_state, i));
        /* Pop the next arg using lua_tostring(L, i) and do your print */
      }
      else if (lua_isinteger(lua_state, i))
      {
        output_string.append(std::to_string(lua_tointeger(lua_state, i)));
      }
      else if (lua_isnumber(lua_state, i))
      {
        output_string.append(std::to_string(lua_tonumber(lua_state, i)));
      }
      else if (lua_isboolean(lua_state, i))
      {
        output_string.append(lua_toboolean(lua_state, i) ? "true" : "false");
      }
      else if (lua_isnil(lua_state, i))
      {
        output_string.append("nil");
      }
      else
      {
        luaL_error(lua_state, "Error: Unknown type encountered in print function. Supported types "
                              "are String, Integer, Number, Boolean, and nil");
      }
    }

    (*print_callback)(output_string);

    return 0;
  }

  bool ShouldCallEndScriptFunction() {
    if (finished_with_global_code && frame_callback_locations.size() == 0 &&
        gc_controller_input_polled_callback_locations.size() == 0 &&
        wii_controller_input_polled_callback_locations.size() == 0 &&
        map_of_instruction_address_to_lua_callback_locations.size() == 0 &&
        map_of_memory_address_read_from_to_lua_callback_locations.size() == 0 &&
        map_of_memory_address_written_to_to_lua_callback_locations.size() == 0)
      return true;
    return false;

  }
  LuaScriptContext(int new_unique_script_identifier, const std::string& new_script_filename,
                   std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                   const std::string& api_version,
                   std::function<void(const std::string&)>* new_print_callback,
                   std::function<void(int)>* new_script_end_callback)
      : ScriptContext(new_unique_script_identifier, new_script_filename,
                      new_pointer_to_list_of_all_scripts)
  {
    if (!set_print_callback)
    {
      print_callback = new_print_callback;
      set_print_callback = true;
    }
    if (!set_script_end_callback)
    {
      script_end_callback = new_script_end_callback;
      set_script_end_callback = true;
    }
    index_of_next_frame_callback_to_execute = 0;

    const std::lock_guard<std::mutex> lock(script_specific_lock);
    main_lua_thread = luaL_newstate();
    current_lua_state_thread = main_lua_thread;
    luaL_openlibs(main_lua_thread);
    std::string executionString =
        (std::string("package.path = package.path .. ';") + File::GetUserPath(D_LOAD_IDX) +
         "LuaLibs/?.lua;" + File::GetSysDirectory() + "LuaLibs/?.lua;'");
    std::replace(executionString.begin(), executionString.end(), '\\', '/');
    luaL_dostring(main_lua_thread, executionString.c_str());
    lua_pushlightuserdata(main_lua_thread, this);
    lua_setglobal(main_lua_thread, THIS_VARIABLE_NAME);

    lua_newtable(main_lua_thread);
    lua_pushcfunction(main_lua_thread, CustomPrintFunction);
    lua_setglobal(main_lua_thread, "print");
    this->ImportModule("importModule", api_version);
    this->ImportModule("OnFrameStart", api_version);
    this->ImportModule("OnGCControllerPolled", api_version);
    this->ImportModule("OnInstructionHit", api_version);
    this->ImportModule("OnMemoryAddressReadFrom", api_version);
    this->ImportModule("OnMemoryAddressWrittenTo", api_version);
    this->ImportModule("OnWiiInputPolled", api_version);

    frame_callback_lua_thread = lua_newthread(main_lua_thread);
    instruction_address_hit_callback_lua_thread = lua_newthread(main_lua_thread);
    memory_address_read_from_callback_lua_thread = lua_newthread(main_lua_thread);
    memory_address_written_to_callback_lua_thread = lua_newthread(main_lua_thread);
    gc_controller_input_polled_callback_lua_thread = lua_newthread(main_lua_thread);
    wii_input_polled_callback_lua_thread = lua_newthread(main_lua_thread);

    if (luaL_loadfile(main_lua_thread,
                      script_filename.c_str()) != LUA_OK)
    {
      const char* temp_string = lua_tostring(main_lua_thread, -1);
      (*print_callback)(temp_string);
      (*script_end_callback)(unique_script_identifier);
    }
    int retVal = lua_resume(main_lua_thread, nullptr, 0, &Lua::x);
    if (retVal == LUA_YIELD)
      called_yielding_function_in_last_global_script_resume = true;
    else
    {
      called_yielding_function_in_last_global_script_resume = false;
      if (retVal == LUA_OK)
      {
        finished_with_global_code = true;
        if (ShouldCallEndScriptFunction())
          (*script_end_callback)(unique_script_identifier);
      }
      else
      {
        if (retVal == 2)
        {
          const char* error_msg = lua_tostring(main_lua_thread, -1);
          (*print_callback)(error_msg);
        }
        (*script_end_callback)(unique_script_identifier);
        is_script_active = false;
      }
    }
  }

  ~LuaScriptContext() {}
  virtual void ImportModule(const std::string& api_name, const std::string& api_version);
  virtual void RunGlobalScopeCode();
  virtual void RunOnFrameStartCallbacks();
  virtual void RunOnGCControllerPolledCallbacks();
  virtual void RunOnInstructionReachedCallbacks(size_t current_address);
  virtual void RunOnMemoryAddressReadFromCallbacks(size_t current_memory_address);
  virtual void RunOnMemoryAddressWrittenToCallbacks(size_t current_memory_address);
  virtual void RunOnWiiInputPolledCallbacks();

  virtual void* RegisterOnFrameStartCallbacks(void* callbacks);
  virtual bool UnregisterOnFrameStartCallbacks(void* callbacks);

  virtual void* RegisterOnGCCControllerPolledCallbacks(void* callbacks);
  virtual bool UnregisterOnGCControllerPolledCallbacks(void* callbacks);

  virtual void* RegisterOnInstructionReachedCallbacks(size_t address, void* callbacks);
  virtual bool UnregisterOnInstructionReachedCallbacks(size_t address, void* callbacks);

  virtual void* RegisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                         void* callbacks);
  virtual bool UnregisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                           void* callbacks);

  virtual void* RegisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                          void* callbacks);
  virtual bool UnregisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                            void* callbacks);

  virtual void* RegisterOnWiiInputPolledCallbacks(void* callbacks);
  virtual bool UnregisterOnWiiInputPolledCallbacks(void* callbacks);

  private:
  void GenericRunCallbacksHelperFunction(lua_State*& current_lua_state,
                                         std::vector<int>& vector_of_callbacks,
                                         int& index_of_next_callback_to_run,
                                         bool& yielded_on_last_callback_call,
                                         bool yields_are_allowed);

  void* RegisterForVectorHelper(std::vector<int>& input_vector, void* callbacks);
  bool UnregisterForVectorHelper(std::vector<int>& input_vector, void* callbacks);

  void* RegisterForMapHelper(size_t address, std::unordered_map<size_t, std::vector<int>>& input_map, void* callbacks);
  bool UnregisterForMapHelper(size_t address, std::unordered_map<size_t, std::vector<int>>& input_map, void* callbacks);
  virtual void ShutdownScript();
};
}  // namespace Scripting
