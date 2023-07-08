#ifndef LUA_SCRIPT_CONTEXT_H
#define LUA_SCRIPT_CONTEXT_H

#include "../DolphinDefinedAPIs.h"
#include "../HelperFiles/ClassMetadata.h"
#include "../HelperFiles/MemoryAddressCallbackTriple.h"
#include <atomic>
#include <unordered_map>
#include <vector>
#include <queue>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


extern const char* THIS_SCRIPT_CONTEXT_VARIABLE_NAME; // Used to access the associated ScriptContext* from inside of a Lua cothread
extern const char* THIS_LUA_SCRIPT_CONTEXT_VARIABLE_NAME; // Used to access the associated LuaScriptContext* from inside of a Lua cothread.

struct LuaScriptContext
{
  LuaScriptContext() {
    base_script_context_ptr = nullptr;
    main_lua_thread = frame_callback_lua_thread = instruction_address_hit_callback_lua_thread = memory_address_read_from_callback_lua_thread =
      memory_address_written_to_callback_lua_thread = gc_controller_input_polled_callback_lua_thread = wii_input_polled_callback_lua_thread =
      button_callback_thread = nullptr;
    index_of_next_frame_callback_to_execute = 0;
  }
  void* base_script_context_ptr;
  lua_State* main_lua_thread;
  lua_State* frame_callback_lua_thread;
  lua_State* instruction_address_hit_callback_lua_thread;
  lua_State* memory_address_read_from_callback_lua_thread;
  lua_State* memory_address_written_to_callback_lua_thread;
  lua_State* gc_controller_input_polled_callback_lua_thread;
  lua_State* wii_input_polled_callback_lua_thread;
  lua_State* button_callback_thread;

  std::queue<int> button_callbacks_to_run;
  std::vector<int> frame_callback_locations;
  std::vector<int> gc_controller_input_polled_callback_locations;
  std::vector<int> wii_controller_input_polled_callback_locations;

  int index_of_next_frame_callback_to_execute;

  std::unordered_map<unsigned int, std::vector<int> > map_of_instruction_address_to_lua_callback_locations;

  std::vector<MemoryAddressCallbackTriple> memory_address_read_from_callback_list;

  std::vector<MemoryAddressCallbackTriple> memory_address_written_to_callback_list;

  std::unordered_map<long long, int> map_of_button_id_to_callback;

  std::atomic<size_t> number_of_frame_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_gc_controller_input_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_wii_input_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_instruction_address_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_memory_address_read_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_memory_address_write_callbacks_to_auto_deregister;

  ClassMetadata class_metadata_buffer;
  FunctionMetadata single_function_metadata_buffer; // Unused - but allows for FunctionMetadatas to be run, if the DLLFunctionMetadataCopyHook_impl() is ever used
};

void* Init_LuaScriptContext_impl(void* new_base_script_context_ptr); // Creates + returns a new LuaScriptContext object, with all fields properly initialized from the base ScriptContext* passed into the function
void Destroy_LuaScriptContext_impl(void* base_script_context_ptr); // Takes as input a ScriptContext*, and frees the memory associated with it.
int CustomPrintFunction(lua_State* lua_state);
bool ShouldCallEndScriptFunction(LuaScriptContext* lua_script_ptr);


// See the declarations in the DLL_Defined_ScriptContext_APIs struct in the ScriptContext_APIs.h file for documentation on what these functions do.
// Note that each of these functions takes as input for its 1st parameter an opaque void* handle to a ScriptContext* (in order to be callable from the Dolphin side).
// However, you can use the get_derived_script_context_class_ptr() method on the ScriptContext* in order to get the LuaScriptContext* associated with the ScriptContext* object.

void ImportModule_impl(void*, const char*, const char*);

void StartScript_impl(void*);

void RunGlobalScopeCode_impl(void*);
void RunOnFrameStartCallbacks_impl(void*);
void RunOnGCControllerPolledCallbacks_impl(void*);
void RunOnWiiInputPolledCallbacks_impl(void*);
void RunButtonCallbacksInQueue_impl(void*);
void RunOnInstructionReachedCallbacks_impl(void*, unsigned int);
void RunOnMemoryAddressReadFromCallbacks_impl(void*, unsigned int);
void RunOnMemoryAddressWrittenToCallbacks_impl(void*, unsigned int);


void* RegisterOnFrameStartCallback_impl(void*, void*);
void RegisterOnFrameStartWithAutoDeregistrationCallback_impl(void*, void*);
int UnregisterOnFrameStartCallback_impl(void*, void*);

void* RegisterOnGCControllerPolledCallback_impl(void*, void*);
void RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl(void*, void*);
int UnregisterOnGCControllerPolledCallback_impl(void*, void*);

void* RegisterOnWiiInputPolledCallback_impl(void*, void*);
void RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl(void*, void*);
int UnregisterOnWiiInputPolledCallback_impl(void*, void*);

void RegisterButtonCallback_impl(void*, long long, void*);
int IsButtonRegistered_impl(void*, long long);
void GetButtonCallbackAndAddToQueue_impl(void*, long long);

void* RegisterOnInstructionReachedCallback_impl(void*, unsigned int, void*);
void RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl(void*, unsigned int, void*);
int UnregisterOnInstructionReachedCallback_impl(void*, void*);

void* RegisterOnMemoryAddressReadFromCallback_impl(void*, unsigned int, unsigned int, void*);
void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void*, unsigned int, unsigned int, void*);
int UnregisterOnMemoryAddressReadFromCallback_impl(void*, void*);

void* RegisterOnMemoryAddressWrittenToCallback_impl(void*, unsigned int, unsigned int, void*);
void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void*, unsigned int, unsigned int, void*);
int UnregisterOnMemoryAddressWrittenToCallback_impl(void*, void*); 

void DLLClassMetadataCopyHook_impl(void*, void*);
void DLLFunctionMetadataCopyHook_impl(void*, void*);



#endif
