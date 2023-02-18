#ifndef LUA_SCRIPT_CONTEXT
#define LUA_SCRIPT_CONTEXT

#include <lua.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

namespace Lua
{

struct LuaScriptContext
{
  int unique_script_identifier;
  std::string script_filename;
  bool finished_with_global_code;


  std::mutex lua_script_specific_lock;

  lua_State* main_lua_thread;
  lua_State* frame_callback_lua_thread;
  lua_State* instruction_address_hit_callback_lua_thread;
  lua_State* memory_address_read_from_callback_lua_thread;
  lua_State* memory_address_written_to_callback_lua_thread;

  bool called_yielding_function_in_last_global_script_resume;
  bool called_yielding_function_in_last_frame_callback_script_resume;
  bool called_yielding_function_in_last_instruction_address_callback_script_resume;
  bool called_yielding_function_in_last_memory_address_read_callback_script_resume;
  bool called_yielding_function_in_last_memory_address_written_callback_script_resume;

  std::vector<int> frame_callback_locations;

  int index_of_next_frame_callback_to_execute;

  std::unordered_map<size_t, std::vector<int>>
      map_of_instruction_address_to_lua_callback_locations;

  std::unordered_map<size_t, std::vector<int>>
      map_of_memory_address_read_from_to_lua_callback_locations;

  std::unordered_map<size_t, std::vector<int>>
      map_of_memory_address_written_to_to_lua_callback_locations;

  bool is_lua_script_active;
  };

  std::shared_ptr<LuaScriptContext>
  CreateNewLuaScriptContext(lua_State* new_main_lua_state, int new_unique_script_identifier,
                            const std::string& new_script_filename);
  }
#endif
