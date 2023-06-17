#include "LuaScriptContext.h"
#include <string>

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
  new_lua_script_context_ptr->map_of_memory_address_written_to_to_lua_callback_locations = std::unordered_map<unsigned int, std::vector<int>();
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

  (void*)(*ImportModuleFunc)(void*, const char*, const char*) = ((DLL_Defined_ScriptContext_APIs*)dolphinDefinedScriptContext_APIs.get_dll_defined_script_context_apis(new_lua_script_context_ptr->base_script_context_ptr))->ImportModule;
  const char* most_recent_script_version = dolphinDefinedScriptContext_APIs.get_script_version();

  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "dolphin", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnFrameStart", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnGCControllerPolled", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnInstructionHit", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnMemoryAddressReadFrom", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnMemoryAddressWrittenTo", most_recent_script_version);
  ImportModuleFunc(new_lua_script_context_ptr->base_script_context_ptr, "OnWiiInputPolled", most_recent_script_version);

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
    dolphinDefinedScriptContext_APIs.get_print_callback_function(new_lua_script_context_ptr->base_script_context_ptr)(temp_string);
    dolphinDefinedScriptContext_APIs.get_script_end_callback_function(new_lua_script_context_ptr->base_script_context_ptr)(dolphinDefinedScriptContext_APIs.get_unique_script_identifier(new_lua_script_context_ptr->base_script_context_ptr));
  }
  else
  {
    dolphinDefinedScriptContext_APIs.add_script_to_queue_of_scripts_waiting_to_start(new_lua_script_context_ptr->base_script_context_ptr);
  }

  return new_lua_script_context_ptr;
}
