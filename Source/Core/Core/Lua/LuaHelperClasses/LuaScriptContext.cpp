#include "Core/Lua/LuaHelperClasses/LuaScriptContext.h"

namespace Lua
{
std::shared_ptr<LuaScriptContext> CreateNewLuaScriptContext(lua_State* new_main_lua_state,
                                                            int new_unique_script_identifier,
                                                            const std::string& new_script_filename)
{
  std::shared_ptr<LuaScriptContext> return_result = std::make_unique<LuaScriptContext>();
  return_result->unique_script_identifier = new_unique_script_identifier;
  return_result->script_filename = new_script_filename;
  return_result->finished_with_global_code = false;
  return_result->current_call_location = Scripting::ScriptCallLocations::FromScriptStartup;
  return_result->called_yielding_function_in_last_global_script_resume = false;
  return_result->called_yielding_function_in_last_frame_callback_script_resume = false;
  return_result->called_yielding_function_in_last_instruction_address_callback_script_resume =
      false;
  return_result->called_yielding_function_in_last_memory_address_read_callback_script_resume =
      false;
  return_result->called_yielding_function_in_last_memory_address_written_callback_script_resume =
      false;
  return_result->called_yielding_function_in_last_gc_controller_input_polled_callback_script_resume = false;
  return_result->main_lua_thread = lua_newthread(new_main_lua_state);
  return_result->frame_callback_lua_thread = nullptr;
  return_result->gc_controller_input_polled_callback_lua_thread = nullptr;
  return_result->instruction_address_hit_callback_lua_thread = nullptr;
  return_result->memory_address_read_from_callback_lua_thread = nullptr;
  return_result->memory_address_written_to_callback_lua_thread = nullptr;
  return_result->frame_callback_locations = std::vector<int>();
  return_result->gc_controller_input_polled_callback_locations = std::vector<int>();
  return_result->index_of_next_frame_callback_to_execute = 0;
  return_result->map_of_instruction_address_to_lua_callback_locations =
      std::unordered_map<size_t, std::vector<int>>();
  return_result->map_of_memory_address_read_from_to_lua_callback_locations =
      std::unordered_map<size_t, std::vector<int>>();
  return_result->map_of_memory_address_written_to_to_lua_callback_locations =
      std::unordered_map<size_t, std::vector<int>>();
  return_result->is_lua_script_active = true;
  return return_result;
}
}  // namespace Lua
