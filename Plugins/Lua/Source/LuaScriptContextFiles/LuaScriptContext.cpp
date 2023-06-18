#include "LuaScriptContext.h"
#include <string>

#include "../CopiedScriptContextFiles/Enums/ScriptCallLocations.h"

const char* THIS_SCRIPT_CONTEXT_VARIABLE_NAME = "THIS_SCRIPT_CONTEXT_13237122LQ7";
const char* THIS_LUA_SCRIPT_CONTEXT_VARIABLE_NAME = "THIS_LUA_SCRIPT_CONTEXT_98485237889G3Jk";

void* Init_LuaScriptContext_impl(void* new_base_script_context_ptr)
{
  LuaScriptContext* new_lua_script_context_ptr = new LuaScriptContext();
  new_lua_script_context_ptr->base_script_context_ptr = new_base_script_context_ptr;
  dolphinDefinedScriptContext_APIs.set_dll_script_context_ptr(new_lua_script_context_ptr->base_script_context_ptr, new_lua_script_context_ptr);
  new_lua_script_context_ptr->button_callbacks_to_run = std::queue<int>();
  new_lua_script_context_ptr->frame_callback_locations = std::vector<int>();
  new_lua_script_context_ptr->gc_controller_input_polled_callback_locations = std::vector<int>();
  new_lua_script_context_ptr->wii_controller_input_polled_callback_locations = std::vector<int>();

  new_lua_script_context_ptr->index_of_next_frame_callback_to_execute = 0;

  new_lua_script_context_ptr->map_of_instruction_address_to_lua_callback_locations = std::unordered_map<unsigned int, std::vector<int>>();
  new_lua_script_context_ptr->map_of_memory_address_read_from_to_lua_callback_locations = std::unordered_map<unsigned int, std::vector<int>>();
  new_lua_script_context_ptr->map_of_memory_address_written_to_to_lua_callback_locations = std::unordered_map<unsigned int, std::vector<int>>();
  new_lua_script_context_ptr->map_of_button_id_to_callback = std::unordered_map<long long, int>();

  new_lua_script_context_ptr->number_of_frame_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_gc_controller_input_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_wii_input_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_instruction_address_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_memory_address_read_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_memory_address_write_callbacks_to_auto_deregister = 0;


  new_lua_script_context_ptr->main_lua_thread = luaL_newstate();
  luaL_openlibs(new_lua_script_context_ptr->main_lua_thread);
  std::string executionString =
    (std::string("package.path = package.path .. ';") + fileUtility_APIs.GetUserPath() +
      "LuaLibs/?.lua;" + fileUtility_APIs.GetSystemDirectory() + "LuaLibs/?.lua;'");
  std::replace(executionString.begin(), executionString.end(), '\\', '/');
  luaL_dostring(new_lua_script_context_ptr->main_lua_thread, executionString.c_str());

  lua_pushlightuserdata(new_lua_script_context_ptr->main_lua_thread, new_lua_script_context_ptr->base_script_context_ptr);
  lua_setglobal(new_lua_script_context_ptr->main_lua_thread, THIS_SCRIPT_CONTEXT_VARIABLE_NAME);

  lua_pushlightuserdata(new_lua_script_context_ptr->main_lua_thread, new_lua_script_context_ptr);
  lua_setglobal(new_lua_script_context_ptr->main_lua_thread, THIS_LUA_SCRIPT_CONTEXT_VARIABLE_NAME);

  lua_newtable(new_lua_script_context_ptr->main_lua_thread);
  lua_pushcfunction(new_lua_script_context_ptr->main_lua_thread, CustomPrintFunction);
  lua_setglobal(new_lua_script_context_ptr->main_lua_thread, "print");

  void (*ImportModuleFuncPtr)(void*, const char*, const char*) = ((DLL_Defined_ScriptContext_APIs*)dolphinDefinedScriptContext_APIs.get_dll_defined_script_context_apis(new_lua_script_context_ptr->base_script_context_ptr))->ImportModule;
  const char* most_recent_script_version = dolphinDefinedScriptContext_APIs.get_script_version();

  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "dolphin", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnFrameStart", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnGCControllerPolled", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnInstructionHit", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnMemoryAddressReadFrom", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnMemoryAddressWrittenTo", most_recent_script_version);
  ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, "OnWiiInputPolled", most_recent_script_version);

  new_lua_script_context_ptr->frame_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->instruction_address_hit_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->memory_address_read_from_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->memory_address_written_to_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->gc_controller_input_polled_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->wii_input_polled_callback_lua_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);
  new_lua_script_context_ptr->button_callback_thread = lua_newthread(new_lua_script_context_ptr->main_lua_thread);

  if (luaL_loadfile(new_lua_script_context_ptr->main_lua_thread, dolphinDefinedScriptContext_APIs.get_script_filename(new_lua_script_context_ptr->base_script_context_ptr)) != LUA_OK)
  {
    const char* temp_string = lua_tostring(new_lua_script_context_ptr->main_lua_thread, -1);
    dolphinDefinedScriptContext_APIs.get_print_callback_function(new_lua_script_context_ptr->base_script_context_ptr)(new_lua_script_context_ptr->base_script_context_ptr, temp_string);
    dolphinDefinedScriptContext_APIs.get_script_end_callback_function(new_lua_script_context_ptr->base_script_context_ptr)(new_lua_script_context_ptr->base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(new_lua_script_context_ptr->base_script_context_ptr));
  }
  else
  {
    dolphinDefinedScriptContext_APIs.add_script_to_queue_of_scripts_waiting_to_start(new_lua_script_context_ptr->base_script_context_ptr);
  }

  return new_lua_script_context_ptr;
}

void unref_vector(LuaScriptContext* lua_script_ptr, std::vector<int>& input_vector)
{
  for (auto& func_ref : input_vector)
    luaL_unref(lua_script_ptr->main_lua_thread, LUA_REGISTRYINDEX, func_ref);
  input_vector.clear();
}

void unref_map(LuaScriptContext* lua_script_ptr, std::unordered_map<unsigned int, std::vector<int>>& input_map)
{
  for (auto& addr_func_pair : input_map)
    unref_vector(lua_script_ptr, addr_func_pair.second);
  input_map.clear();
}

void unref_map(LuaScriptContext* lua_script_ptr, std::unordered_map<long long, int>& input_map)
{
  for (auto& identifier_func_pair : input_map)
    luaL_unref(lua_script_ptr->main_lua_thread, LUA_REGISTRYINDEX, identifier_func_pair.second);
  input_map.clear();
}

void Destroy_LuaScriptContext_impl(void* base_script_context_ptr) // Takes as input a ScriptContext*, and frees the memory associated with it.
{
  LuaScriptContext* lua_script_ptr = (LuaScriptContext*) dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr;
  unref_vector(lua_script_ptr, lua_script_ptr->frame_callback_locations);
  unref_vector(lua_script_ptr, lua_script_ptr->gc_controller_input_polled_callback_locations);
  unref_vector(lua_script_ptr, lua_script_ptr->wii_controller_input_polled_callback_locations);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_instruction_address_to_lua_callback_locations);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_memory_address_read_from_to_lua_callback_locations);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_memory_address_written_to_to_lua_callback_locations);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_button_id_to_callback);
  delete lua_script_ptr;
}

int CustomPrintFunction(lua_State* lua_state)
{
  int nargs = lua_gettop(lua_state);
  std::string output_string;
  for (int i = 1; i <= nargs; i++)
  {
    if (lua_isstring(lua_state, i))
    {
      output_string.append(lua_tostring(lua_state, i));
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

  lua_getglobal(lua_state, THIS_SCRIPT_CONTEXT_VARIABLE_NAME);
  void* corresponding_script_context =  (void*)lua_touserdata(lua_state, -1);
  lua_pop(lua_state, 1);

  dolphinDefinedScriptContext_APIs.get_print_callback_function(corresponding_script_context)(corresponding_script_context, output_string.c_str());

  return 0;
}

int getNumberOfCallbacksInMap(std::unordered_map<unsigned int, std::vector<int>>& input_map)
{
  int return_val = 0;
  for (auto& element : input_map)
  {
    return_val += (int)element.second.size();
  }
  return return_val;
}

bool ShouldCallEndScriptFunction(LuaScriptContext* lua_script_ptr)
{
  if (dolphinDefinedScriptContext_APIs.get_is_finished_with_global_code(lua_script_ptr->base_script_context_ptr) &&
    (lua_script_ptr->frame_callback_locations.size() == 0 ||
      lua_script_ptr->frame_callback_locations.size() - lua_script_ptr->number_of_frame_callbacks_to_auto_deregister <= 0) &&
    (lua_script_ptr->gc_controller_input_polled_callback_locations.size() == 0 ||
      lua_script_ptr->gc_controller_input_polled_callback_locations.size() -
        lua_script_ptr->number_of_gc_controller_input_callbacks_to_auto_deregister <=
      0) &&
    (lua_script_ptr->wii_controller_input_polled_callback_locations.size() == 0 ||
      lua_script_ptr->wii_controller_input_polled_callback_locations.size() -
      lua_script_ptr->number_of_wii_input_callbacks_to_auto_deregister <=
      0) &&
    (lua_script_ptr->map_of_instruction_address_to_lua_callback_locations.size() == 0 ||
      getNumberOfCallbacksInMap(lua_script_ptr->map_of_instruction_address_to_lua_callback_locations) -
      lua_script_ptr->number_of_instruction_address_callbacks_to_auto_deregister <=
      0) &&
    (lua_script_ptr->map_of_memory_address_read_from_to_lua_callback_locations.size() == 0 ||
      getNumberOfCallbacksInMap(lua_script_ptr->map_of_memory_address_read_from_to_lua_callback_locations) -
      lua_script_ptr->number_of_memory_address_read_callbacks_to_auto_deregister <=
      0) &&
    (lua_script_ptr->map_of_memory_address_written_to_to_lua_callback_locations.size() == 0 ||
      getNumberOfCallbacksInMap(lua_script_ptr->map_of_memory_address_written_to_to_lua_callback_locations) -
      lua_script_ptr->number_of_memory_address_write_callbacks_to_auto_deregister <=
      0) &&
    lua_script_ptr->button_callbacks_to_run.empty())
    return true;
  return false;
}

void ImportModule_impl(void* base_script_ptr, const char* api_name, const char* api_version)
{
  LuaScriptContext* lua_script_ptr = reinterpret_cast<LuaScriptContext*>(dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr(base_script_ptr));
  ScriptCallLocations call_location = (ScriptCallLocations)(dolphinDefinedScriptContext_APIs.get_script_call_location(base_script_ptr));
  lua_State* current_lua_thread = lua_script_ptr->main_lua_thread;

  if (call_location == ScriptCallLocations::FromFrameStartCallback)
    current_lua_thread = lua_script_ptr->frame_callback_lua_thread;

  else if (call_location == ScriptCallLocations::FromInstructionHitCallback)
    current_lua_thread = lua_script_ptr->instruction_address_hit_callback_lua_thread;

  else if (call_location == ScriptCallLocations::FromMemoryAddressReadFromCallback)
    current_lua_thread = lua_script_ptr->memory_address_read_from_callback_lua_thread;

  else if (call_location == ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    current_lua_thread = lua_script_ptr->memory_address_written_to_callback_lua_thread;

  else if (call_location == ScriptCallLocations::FromGCControllerInputPolled)
    current_lua_thread = lua_script_ptr->gc_controller_input_polled_callback_lua_thread;

  else if (call_location == ScriptCallLocations::FromWiiInputPolled)
    current_lua_thread = lua_script_ptr->wii_input_polled_callback_lua_thread;

}
