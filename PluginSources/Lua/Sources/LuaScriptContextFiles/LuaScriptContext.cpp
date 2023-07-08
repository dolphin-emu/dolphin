#include "LuaScriptContext.h"
#include <memory>
#include <string>
#include <map>


#include "../CopiedScriptContextFiles/Enums/GCButtonNameEnum.h"
#include "../CopiedScriptContextFiles/Enums/ScriptCallLocations.h"
#include "../CopiedScriptContextFiles/Enums/ScriptReturnCodes.h"

const char* THIS_SCRIPT_CONTEXT_VARIABLE_NAME = "THIS_SCRIPT_CONTEXT_13237122LQ7";
const char* THIS_LUA_SCRIPT_CONTEXT_VARIABLE_NAME = "THIS_LUA_SCRIPT_CONTEXT_98485237889G3Jk";

static std::vector<int> digital_buttons_list = std::vector<int>();
static std::vector<int> analog_buttons_list = std::vector<int>();

static int zero_int = 0;

void callScriptEndCallbackFunction(void* base_script_context_ptr)
{
  dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
}

void* Init_LuaScriptContext_impl(void* new_base_script_context_ptr)
{
  if (digital_buttons_list.empty())
  {
    int raw_button_enum = 0;
    do
    {
      if (gcButton_APIs.IsDigitalButton(raw_button_enum))
        digital_buttons_list.push_back(raw_button_enum);
      else if (gcButton_APIs.IsAnalogButton(raw_button_enum))
        analog_buttons_list.push_back(raw_button_enum);
      raw_button_enum++;
    } while (gcButton_APIs.IsValidButtonEnum(raw_button_enum));
  }

  LuaScriptContext* new_lua_script_context_ptr = new LuaScriptContext();
  new_lua_script_context_ptr->base_script_context_ptr = new_base_script_context_ptr;
  dolphinDefinedScriptContext_APIs.set_derived_script_context_ptr(new_lua_script_context_ptr->base_script_context_ptr, new_lua_script_context_ptr);
  new_lua_script_context_ptr->button_callbacks_to_run = std::queue<int>();
  new_lua_script_context_ptr->frame_callback_locations = std::vector<int>();
  new_lua_script_context_ptr->gc_controller_input_polled_callback_locations = std::vector<int>();
  new_lua_script_context_ptr->wii_controller_input_polled_callback_locations = std::vector<int>();

  new_lua_script_context_ptr->index_of_next_frame_callback_to_execute = 0;

  new_lua_script_context_ptr->map_of_instruction_address_to_lua_callback_locations = std::unordered_map<unsigned int, std::vector<int> >();
  new_lua_script_context_ptr->memory_address_read_from_callback_list = std::vector<MemoryAddressCallbackTriple>();
  new_lua_script_context_ptr->memory_address_written_to_callback_list = std::vector<MemoryAddressCallbackTriple>();
  new_lua_script_context_ptr->map_of_button_id_to_callback = std::unordered_map<long long, int>();

  new_lua_script_context_ptr->number_of_frame_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_gc_controller_input_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_wii_input_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_instruction_address_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_memory_address_read_callbacks_to_auto_deregister = 0;
  new_lua_script_context_ptr->number_of_memory_address_write_callbacks_to_auto_deregister = 0;

  new_lua_script_context_ptr->class_metadata_buffer = ClassMetadata("", std::vector<FunctionMetadata>());

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

  const void* default_modules = moduleLists_APIs.GetListOfDefaultModules();
  unsigned long long number_of_default_modules = moduleLists_APIs.GetSizeOfList(default_modules);
  for (unsigned long long i = 0; i < number_of_default_modules; ++i)
    ImportModuleFuncPtr(new_lua_script_context_ptr->base_script_context_ptr, moduleLists_APIs.GetElementAtListIndex(default_modules, i), most_recent_script_version);

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

void unref_vector(LuaScriptContext* lua_script_ptr, std::vector<MemoryAddressCallbackTriple>& input_vector)
{
  for (auto& triple : input_vector)
    luaL_unref(lua_script_ptr->main_lua_thread, LUA_REGISTRYINDEX, triple.callback_ref);
  input_vector.clear();
}

void unref_map(LuaScriptContext* lua_script_ptr, std::unordered_map<unsigned int, std::vector<int> >& input_map)
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
  LuaScriptContext* lua_script_ptr = (LuaScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr);
  if (lua_script_ptr == nullptr)
    return;
  unref_vector(lua_script_ptr, lua_script_ptr->frame_callback_locations);
  unref_vector(lua_script_ptr, lua_script_ptr->gc_controller_input_polled_callback_locations);
  unref_vector(lua_script_ptr, lua_script_ptr->wii_controller_input_polled_callback_locations);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_instruction_address_to_lua_callback_locations);
  unref_vector(lua_script_ptr, lua_script_ptr->memory_address_read_from_callback_list);
  unref_vector(lua_script_ptr, lua_script_ptr->memory_address_written_to_callback_list);
  unref_map(lua_script_ptr, lua_script_ptr->map_of_button_id_to_callback);
  delete lua_script_ptr;
  dolphinDefinedScriptContext_APIs.set_derived_script_context_ptr(base_script_context_ptr, nullptr);
}

void SetErrorCode(void* base_script_context_ptr, lua_State* lua_state, const char* error_str)
{
  dolphinDefinedScriptContext_APIs.set_script_return_code(base_script_context_ptr, ScriptingEnums::ScriptReturnCodes::UnknownError);
  dolphinDefinedScriptContext_APIs.set_error_message(base_script_context_ptr, error_str);
  luaL_error(lua_state, error_str);
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
      lua_getglobal(lua_state, THIS_SCRIPT_CONTEXT_VARIABLE_NAME);
      void* base_script_context_ptr = lua_touserdata(lua_state, -1);
      lua_pop(lua_state, 1);
      SetErrorCode(base_script_context_ptr, lua_state, "Error: Unknown type encountered in print function. Supported types "
        "are String, Integer, Number, Boolean, and nil");
      return 0;
    }
  }

  lua_getglobal(lua_state, THIS_SCRIPT_CONTEXT_VARIABLE_NAME);
  void* corresponding_script_context = (void*)lua_touserdata(lua_state, -1);
  lua_pop(lua_state, 1);

  dolphinDefinedScriptContext_APIs.get_print_callback_function(corresponding_script_context)(corresponding_script_context, output_string.c_str());

  return 0;
}

int getNumberOfCallbacksInMap(std::unordered_map<unsigned int, std::vector<int> >& input_map)
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
    (lua_script_ptr->memory_address_read_from_callback_list.size() == 0 ||
      lua_script_ptr->memory_address_read_from_callback_list.size() - 
      lua_script_ptr->number_of_memory_address_read_callbacks_to_auto_deregister <=
      0) &&
    (lua_script_ptr->memory_address_written_to_callback_list.size() == 0 ||
      lua_script_ptr->memory_address_written_to_callback_list.size() -
      lua_script_ptr->number_of_memory_address_write_callbacks_to_auto_deregister <=
      0) &&
    lua_script_ptr->button_callbacks_to_run.empty())
    return true;
  return false;
}

class UserdataClass
{
public:
  inline UserdataClass() {};
};

UserdataClass* GetNewUserdataInstance()
{
  return std::make_shared<UserdataClass>(UserdataClass()).get();
}

int freeAndReturn(void* arg_holder_ptr, int ret_val)
{
  argHolder_APIs.Delete_ArgHolder(arg_holder_ptr);
  return ret_val;
}

void ImportModule_impl(void* base_script_ptr, const char* api_name, const char* api_version)
{
  LuaScriptContext* lua_script_ptr = reinterpret_cast<LuaScriptContext*>(dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_ptr));
  ScriptingEnums::ScriptCallLocations call_location = (ScriptingEnums::ScriptCallLocations)(dolphinDefinedScriptContext_APIs.get_script_call_location(base_script_ptr));
  lua_State* current_lua_thread = lua_script_ptr->main_lua_thread;

  if (call_location == ScriptingEnums::ScriptCallLocations::FromFrameStartCallback)
    current_lua_thread = lua_script_ptr->frame_callback_lua_thread;

  else if (call_location == ScriptingEnums::ScriptCallLocations::FromInstructionHitCallback)
    current_lua_thread = lua_script_ptr->instruction_address_hit_callback_lua_thread;

  else if (call_location == ScriptingEnums::ScriptCallLocations::FromMemoryAddressReadFromCallback)
    current_lua_thread = lua_script_ptr->memory_address_read_from_callback_lua_thread;

  else if (call_location == ScriptingEnums::ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    current_lua_thread = lua_script_ptr->memory_address_written_to_callback_lua_thread;

  else if (call_location == ScriptingEnums::ScriptCallLocations::FromGCControllerInputPolled)
    current_lua_thread = lua_script_ptr->gc_controller_input_polled_callback_lua_thread;

  else if (call_location == ScriptingEnums::ScriptCallLocations::FromWiiInputPolled)
    current_lua_thread = lua_script_ptr->wii_input_polled_callback_lua_thread;

  UserdataClass** userdata_ptr_ptr =
    (UserdataClass**)lua_newuserdata(current_lua_thread, sizeof(UserdataClass*));
  *userdata_ptr_ptr = GetNewUserdataInstance();

  luaL_newmetatable(
    current_lua_thread,
    (std::string("Lua") + char(std::toupper(api_name[0])) + (api_name + 1) + "MetaTable")
    .c_str());
  lua_pushvalue(current_lua_thread, -1);
  lua_setfield(current_lua_thread, -2, "__index");

  lua_script_ptr->class_metadata_buffer = ClassMetadata();
  classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(base_script_ptr, api_name, api_version);
  if (lua_script_ptr->class_metadata_buffer.class_name.empty())
  {
    std::string error_msg = std::string("Error: Could not find the module the user imported with the name ") + api_name;
    SetErrorCode(base_script_ptr, current_lua_thread, error_msg.c_str());
    return;
  }

  std::vector<luaL_Reg> final_lua_functions_list = std::vector<luaL_Reg>();

  for (auto& current_function_metadata : lua_script_ptr->class_metadata_buffer.functions_list)
  {
    lua_CFunction lambdaFunction = [](lua_State* lua_state) mutable {

      std::string class_name = lua_tostring(lua_state, lua_upvalueindex(1));
      FunctionMetadata* localFunctionMetadata =
        (FunctionMetadata*)lua_touserdata(lua_state, lua_upvalueindex(2));
      lua_getglobal(lua_state, THIS_SCRIPT_CONTEXT_VARIABLE_NAME);
      void* base_script_context_ptr = lua_touserdata(lua_state, -1);
      LuaScriptContext* lua_script_context_ptr = (LuaScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr);
      lua_pop(lua_state, 1);
      void* arguments_list_ptr = vectorOfArgHolder_APIs.CreateNewVectorOfArgHolders();
      void* temp_arg_holder_ptr = nullptr;
      void* return_value = nullptr;
      int function_reference = -1;
      signed long long key = 0;
      signed long long value = 0;
      const char* raw_button_name;
      int button_name_as_enum;
      unsigned char button_value = 0;
      signed long long raw_magnitude = 0;
      int garbage_val = 0;
      double point1 = 0.0;
      double point2 = 0.0;
      void* temp_val = nullptr;
      std::string error_message = "";
      std::string part_of_error_message = "";
      void* base_iterator_ptr = nullptr;
      void* travel_iterator_ptr = nullptr;

      if (lua_type(lua_state, 1) != LUA_TUSERDATA)
      {
        error_message = (std::string("Error: User attempted to call the ") + class_name + ":" + localFunctionMetadata->function_name + "() function using the dot "
          "operator. Please use the colon operator instead like this: " + class_name + ":" + localFunctionMetadata->function_name);
        vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return 0;
      }

      if (localFunctionMetadata->return_type == ScriptingEnums::ArgTypeEnum::U64)
      {
        error_message = (std::string("Error: User attempted to call the ") + class_name + ":" + localFunctionMetadata->function_name + "() function, which returns a "
          "u64. The largest size type that Dolphin supports for Lua is s64. As "
          "such, please use an s64 version of this function instead.");
        vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return 0;
      }

      int next_index_in_args = 2;

      for (ScriptingEnums::ArgTypeEnum arg_type : localFunctionMetadata->arguments_list)
      {
        switch (arg_type)
        {
        case ScriptingEnums::ArgTypeEnum::Boolean:
          temp_arg_holder_ptr = argHolder_APIs.CreateBoolArgHolder(lua_toboolean(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::U8:
          temp_arg_holder_ptr = argHolder_APIs.CreateU8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::U16:
          temp_arg_holder_ptr = argHolder_APIs.CreateU16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::U32:
          temp_arg_holder_ptr = argHolder_APIs.CreateU32ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::U64:
          error_message = std::string("Error: User attempted to call the ") + class_name + ":" + localFunctionMetadata->function_name + "() function, which takes a "
            "u64 as one of its parameters. The largest type supported in Dolphin "
            "for Lua is s64. Please use an s64 version of the function instead.";
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
          SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
          return 0;

        case ScriptingEnums::ArgTypeEnum::S8:
          temp_arg_holder_ptr = argHolder_APIs.CreateS8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::S16:
          temp_arg_holder_ptr = argHolder_APIs.CreateS16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::S32:
          temp_arg_holder_ptr = argHolder_APIs.CreateS32ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::S64:
          temp_arg_holder_ptr = argHolder_APIs.CreateS64ArgHolder(luaL_checkinteger(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::Float:
          temp_arg_holder_ptr = argHolder_APIs.CreateFloatArgHolder(luaL_checknumber(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::Double:
          temp_arg_holder_ptr = argHolder_APIs.CreateDoubleArgHolder(luaL_checknumber(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::String:
          temp_arg_holder_ptr = argHolder_APIs.CreateStringArgHolder(luaL_checkstring(lua_state, next_index_in_args));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::RegistrationInputType:
          lua_pushvalue(lua_state, next_index_in_args);
          function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          temp_arg_holder_ptr = argHolder_APIs.CreateRegistrationInputTypeArgHolder(*((void**)(&function_reference)));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationInputType:
          lua_pushvalue(lua_state, next_index_in_args);
          function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          temp_arg_holder_ptr = argHolder_APIs.CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(*((void**)(&function_reference)));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::RegistrationForButtonCallbackInputType:
          if (!((DLL_Defined_ScriptContext_APIs*)dolphinDefinedScriptContext_APIs.get_dll_defined_script_context_apis(base_script_context_ptr))->IsButtonRegistered(base_script_context_ptr,
            argHolder_APIs.GetS64FromArgHolder(vectorOfArgHolder_APIs.GetArgumentForVectorOfArgHolders(arguments_list_ptr, vectorOfArgHolder_APIs.GetSizeOfVectorOfArgHolders(arguments_list_ptr) - 1))))
            // this is a terrible hack - but it works to prevent a
            // button callback from being added to Lua's stack once
            // every frame.
          {
            lua_pushvalue(lua_state, next_index_in_args);
            function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          }
          else
          {
            function_reference = -1;
          }
          temp_arg_holder_ptr = argHolder_APIs.CreateRegistrationForButtonCallbackInputTypeArgHolder(*((void**)(&function_reference)));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::UnregistrationInputType:
          function_reference = lua_tointeger(lua_state, next_index_in_args);
          luaL_unref(lua_state, LUA_REGISTRYINDEX, function_reference);
          temp_arg_holder_ptr = argHolder_APIs.CreateUnregistrationInputTypeArgHolder(*((void**)(&function_reference)));
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::AddressToByteMap:
          temp_arg_holder_ptr = argHolder_APIs.CreateAddressToByteMapArgHolder();
          lua_pushnil(lua_state);
          while (lua_next(lua_state, next_index_in_args))
          {
            key = lua_tointeger(lua_state, -2);
            value = lua_tointeger(lua_state, -1);

            if (key < 0)
            {
              error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + "() function, address of byte was less than 0!";
              argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
              vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
              SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
              return 0;
            }

            if (value < -128)
            {
              error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + "() function, value of byte was less than "
                "-128, which can't be represented by 1 byte!";
              argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
              vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
              SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
              return 0;
            }
            if (value > 255)
            {
              error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + "() function, value of byte was more than "
                "255, which can't be represented by 1 byte!";
              argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
              vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
              SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
              return 0;
            }

            argHolder_APIs.AddPairToAddressToByteMapArgHolder(temp_arg_holder_ptr, key, value);
            lua_pop(lua_state, 1);
          }
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::ControllerStateObject:
          lua_pushnil(lua_state);
          temp_arg_holder_ptr = argHolder_APIs.CreateControllerStateArgHolder();
          while (lua_next(lua_state, next_index_in_args) != 0)
          {
            raw_button_name = luaL_checkstring(lua_state, -2);
            if (raw_button_name == nullptr || strlen(raw_button_name) == 0)
            {
              error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + " function, an empty string was passed in "
                "for a button name!";
              argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
              vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
              SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
              return 0;
            }
            button_name_as_enum = gcButton_APIs.ParseGCButton(raw_button_name);
            if (!gcButton_APIs.IsValidButtonEnum(button_name_as_enum))
            {
              error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + " function, an invalid button name of " + raw_button_name + " was passed in "
                "for a button name. Valid button names are: A, B, X, Y, Z, L, R, triggerR, triggerL, analogStickX, analogStickY, cStcikX, cStickY, start, reset, getOrigin and isConnected";
              argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
              vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
              SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
              return 0;
            }
            if (gcButton_APIs.IsDigitalButton(button_name_as_enum))
              button_value = static_cast<unsigned char>(lua_toboolean(lua_state, -1));
            else if (gcButton_APIs.IsAnalogButton(button_name_as_enum))
            {
              raw_magnitude = luaL_checkinteger(lua_state, -1);
              if (raw_magnitude < 0 || raw_magnitude > 255)
              {
                error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + " function, button value for button " + raw_button_name + " was outside the valid bounds of 0-255!";
                argHolder_APIs.Delete_ArgHolder(temp_arg_holder_ptr);
                vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
                SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
                return 0;
              }
              button_value = static_cast<unsigned char>(raw_magnitude);
            }
            argHolder_APIs.SetControllerStateArgHolderValue(temp_arg_holder_ptr, button_name_as_enum, button_value);
            lua_pop(lua_state, 1);
          }
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        case ScriptingEnums::ArgTypeEnum::ListOfPoints:
          temp_arg_holder_ptr = argHolder_APIs.CreateListOfPointsArgHolder();
          lua_pushnil(lua_state);
          while (lua_next(lua_state, next_index_in_args) != 0)
          {
            garbage_val = lua_tointeger(lua_state, -2);
            lua_pushnil(lua_state);
            lua_next(lua_state, -2);
            garbage_val = lua_tointeger(lua_state, -2);
            point1 = lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);
            lua_next(lua_state, -2);
            lua_tointeger(lua_state, -2);
            point2 = lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);
            lua_next(lua_state, -2);
            lua_pop(lua_state, 1);
            argHolder_APIs.ListOfPointsArgHolderPushBack(temp_arg_holder_ptr, point1, point2);
          }
          vectorOfArgHolder_APIs.PushBack(arguments_list_ptr, temp_arg_holder_ptr);
          break;

        default:
          error_message = std::string("Error: Unknown type passed in as argument to ") + localFunctionMetadata->function_name + " function.";
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);
          SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
          return 0;
        }
        ++next_index_in_args;
      }

      return_value = functionMetadata_APIs.RunFunction(localFunctionMetadata->function_pointer, lua_script_context_ptr->base_script_context_ptr, arguments_list_ptr);
      vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(arguments_list_ptr);

      if (return_value == nullptr)
      {
        error_message = std::string("Error: Unknown implementation error occured in ") + class_name + ":" + localFunctionMetadata->function_name + "(). Return value was NULL!";
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return 0;
      }
      if (argHolder_APIs.GetIsEmpty(return_value))
      {
        lua_pushnil(lua_state);
        return freeAndReturn(return_value, 0);
      }

      switch (((ScriptingEnums::ArgTypeEnum)argHolder_APIs.GetArgType(return_value)))
      {

      case ScriptingEnums::ArgTypeEnum::ErrorStringType:
        part_of_error_message = std::string(argHolder_APIs.GetErrorStringFromArgHolder(return_value));
        argHolder_APIs.Delete_ArgHolder(return_value);
        error_message = std::string("Error: in ") + class_name + ":" + localFunctionMetadata->function_name + "() function, " + part_of_error_message;
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return 0;
      case ScriptingEnums::ArgTypeEnum::YieldType:
        argHolder_APIs.Delete_ArgHolder(return_value);
        lua_yield(lua_state, 0);
        return 0;
      case ScriptingEnums::ArgTypeEnum::VoidType:
        return freeAndReturn(return_value, 0);
      case ScriptingEnums::ArgTypeEnum::Boolean:
        lua_pushboolean(lua_state, argHolder_APIs.GetBoolFromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::U8:
        lua_pushinteger(lua_state, static_cast<signed long long>(argHolder_APIs.GetU8FromArgHolder(return_value)));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::U16:
        lua_pushinteger(lua_state, static_cast<signed long long>(argHolder_APIs.GetU16FromArgHolder(return_value)));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::U32:
        lua_pushinteger(lua_state, static_cast<signed long long>(argHolder_APIs.GetU32FromArgHolder(return_value)));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::U64:
        argHolder_APIs.Delete_ArgHolder(return_value);
        error_message = std::string("Error: User called the ") + class_name + ":" + localFunctionMetadata->function_name + "() function, which returned a value of "
          "type u64. The largest value that Dolphin supports for Lua is s64. "
          "Please call an s64 version of the function instead.";
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return 0;
      case ScriptingEnums::ArgTypeEnum::S8:
        lua_pushinteger(lua_state, argHolder_APIs.GetS8FromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::S16:
        lua_pushinteger(lua_state, argHolder_APIs.GetS16FromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::S32:
        lua_pushinteger(lua_state, argHolder_APIs.GetS32FromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::S64:
        lua_pushinteger(lua_state, argHolder_APIs.GetS64FromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::Float:
        lua_pushnumber(lua_state, argHolder_APIs.GetFloatFromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::Double:
        lua_pushnumber(lua_state, argHolder_APIs.GetDoubleFromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::String:
        lua_pushstring(lua_state, argHolder_APIs.GetStringFromArgHolder(return_value));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::RegistrationReturnType:
        temp_val = argHolder_APIs.GetVoidPointerFromArgHolder(return_value);
        lua_pushinteger(lua_state, *((int*)&(temp_val)));
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType:
      case ScriptingEnums::ArgTypeEnum::UnregistrationReturnType:
        return freeAndReturn(return_value, 0);
      case ScriptingEnums::ArgTypeEnum::ShutdownType:
        argHolder_APIs.Delete_ArgHolder(return_value);
        callScriptEndCallbackFunction(base_script_context_ptr);
        return 0;
      case ScriptingEnums::ArgTypeEnum::AddressToByteMap:
        lua_createtable(lua_state, static_cast<int>(argHolder_APIs.GetSizeOfAddressToByteMapArgHolder(return_value)), 0);
        travel_iterator_ptr = base_iterator_ptr = argHolder_APIs.CreateIteratorForAddressToByteMapArgHolder(return_value);
        while (travel_iterator_ptr != nullptr)
        {
          key = argHolder_APIs.GetKeyForAddressToByteMapArgHolder(travel_iterator_ptr);
          value = argHolder_APIs.GetValueForAddressToUnsignedByteMapArgHolder(travel_iterator_ptr);
          lua_pushinteger(lua_state, value);
          lua_rawseti(lua_state, -2, key);
          travel_iterator_ptr = argHolder_APIs.IncrementIteratorForAddressToByteMapArgHolder(travel_iterator_ptr, return_value);
        }
        argHolder_APIs.Delete_IteratorForAddressToByteMapArgHolder(base_iterator_ptr);
        return freeAndReturn(return_value, 1);
      case ScriptingEnums::ArgTypeEnum::ControllerStateObject:
        lua_newtable(lua_state);
        for (int next_digital_button : digital_buttons_list)
        {
          const char* button_name = gcButton_APIs.ConvertButtonEnumToString(next_digital_button);
          lua_pushstring(lua_state, button_name);
          lua_pushboolean(lua_state, argHolder_APIs.GetControllerStateArgHolderValue(return_value, next_digital_button));
          lua_settable(lua_state, -3);
        }
        for (int next_analog_button : analog_buttons_list)
        {
          const char* button_name = gcButton_APIs.ConvertButtonEnumToString(next_analog_button);
          lua_pushstring(lua_state, button_name);
          lua_pushinteger(lua_state, argHolder_APIs.GetControllerStateArgHolderValue(return_value, next_analog_button));
          lua_settable(lua_state, -3);
        }
        return freeAndReturn(return_value, 1);

      default:
        error_message = std::string("Error: unsupported return type encountered in function with value of: ") + std::to_string(argHolder_APIs.GetArgType(return_value));
        SetErrorCode(base_script_context_ptr, lua_state, error_message.c_str());
        return freeAndReturn(return_value, 0);
      }
    };

    FunctionMetadata* heap_function_metadata = new FunctionMetadata(current_function_metadata);
    lua_pushstring(current_lua_thread, heap_function_metadata->function_name.c_str());
    lua_pushstring(current_lua_thread, lua_script_ptr->class_metadata_buffer.class_name.c_str());
    lua_pushlightuserdata(current_lua_thread, heap_function_metadata);
    lua_pushcclosure(current_lua_thread, lambdaFunction, 2);
    lua_settable(current_lua_thread, -3);
  }

  lua_setmetatable(current_lua_thread, -2);
  lua_setglobal(current_lua_thread, api_name);
}

void callPrintFunction(void* base_script_context_ptr, const char* string_to_print)
{
  dolphinDefinedScriptContext_APIs.get_print_callback_function(base_script_context_ptr)(base_script_context_ptr, string_to_print);
}

LuaScriptContext* getLuaScriptContext(void* base_script_context_ptr)
{
  return reinterpret_cast<LuaScriptContext*>(dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr));
}
void StartScript_impl(void* base_script_context_ptr)
{
  if (!dolphinDefinedScriptContext_APIs.get_is_script_active(base_script_context_ptr)) // this variable is set to true when a Script is first created (in its constructor)
  {
    return; // This variable is only set to false when the scriptEnd callback is called - as such, we don't call it a 2nd time.
  }

  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int retVal = lua_resume(lua_script_ptr->main_lua_thread, nullptr, 0, &zero_int);
  if (retVal == LUA_YIELD)
    dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_global_script_resume(base_script_context_ptr, 1);
  else
  {
    dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_global_script_resume(base_script_context_ptr, 0);
    if (retVal == LUA_OK)
    {
      dolphinDefinedScriptContext_APIs.set_is_finished_with_global_code(base_script_context_ptr, 1);
      if (ShouldCallEndScriptFunction(lua_script_ptr))
        callScriptEndCallbackFunction(base_script_context_ptr);
    }
    else
    {
      if (retVal == 2)
      {
        const char* error_msg = lua_tostring(lua_script_ptr->main_lua_thread, -1);
        callPrintFunction(base_script_context_ptr, error_msg);
      }
      callScriptEndCallbackFunction(base_script_context_ptr);
    }
  }
}

bool callEndIfNeededAndReturnTrueIfShouldEnd(void* base_script_context_ptr)
{
  if (!dolphinDefinedScriptContext_APIs.get_is_script_active)
    return true;

  if (ShouldCallEndScriptFunction(getLuaScriptContext(base_script_context_ptr)))
  {
    callScriptEndCallbackFunction(base_script_context_ptr);
    return true;
  }

  return false;
}

void RunGlobalScopeCode_impl(void* base_script_context_ptr)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;

  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);

  if (dolphinDefinedScriptContext_APIs.get_is_finished_with_global_code(base_script_context_ptr))
    return;

  int ret_val = lua_resume(lua_script_ptr->main_lua_thread, nullptr, 0, &zero_int);
  if (ret_val == LUA_YIELD)
    dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_global_script_resume(base_script_context_ptr, 1);
  else
  {
    dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_global_script_resume(base_script_context_ptr, 0);
    if (ret_val == LUA_OK)
      dolphinDefinedScriptContext_APIs.set_is_finished_with_global_code(base_script_context_ptr, 1);
    else
    {
      const char* error_msg = lua_tostring(lua_script_ptr->main_lua_thread, -1);
      callPrintFunction(base_script_context_ptr, error_msg);
      callScriptEndCallbackFunction(base_script_context_ptr);
    }
  }
}

void GenericRunCallbacksHelperFunction(void* base_script_context_ptr, lua_State*& current_lua_state, std::vector<int>& vector_of_callbacks,
  int& index_of_next_callback_to_run, bool is_frame_start_callback)
{
  int ret_val = 0;
  if (!is_frame_start_callback)
    index_of_next_callback_to_run = 0;
  else if (index_of_next_callback_to_run < 0 ||
    index_of_next_callback_to_run >= vector_of_callbacks.size())
    index_of_next_callback_to_run = 0;

  for (index_of_next_callback_to_run; index_of_next_callback_to_run < vector_of_callbacks.size(); ++index_of_next_callback_to_run)
  {
    if (is_frame_start_callback && dolphinDefinedScriptContext_APIs.get_called_yielding_function_in_last_frame_callback_script_resume(base_script_context_ptr))
    {
      ret_val = lua_resume(current_lua_state, nullptr, 0, &zero_int);
    }
    else
    {
      lua_rawgeti(current_lua_state, LUA_REGISTRYINDEX,
        vector_of_callbacks[index_of_next_callback_to_run]);
      ret_val = lua_resume(current_lua_state, nullptr, 0, &zero_int);
    }

    if (ret_val == LUA_YIELD) // The only callback where this is possible is OnFrameStart callbacks.
    {
      dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_frame_callback_script_resume(base_script_context_ptr, 1);
      break;
    }
    else
    {
      if (is_frame_start_callback)
        dolphinDefinedScriptContext_APIs.set_called_yielding_function_in_last_frame_callback_script_resume(base_script_context_ptr, 0);
      if (ret_val != LUA_OK)
      {
        const char* error_msg = lua_tostring(current_lua_state, -1);
        callPrintFunction(base_script_context_ptr, error_msg);
        callScriptEndCallbackFunction(base_script_context_ptr);
        return;
      }
    }
  }
}

void RunOnFrameStartCallbacks_impl(void* base_script_context_ptr)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->frame_callback_lua_thread, lua_script_ptr->frame_callback_locations, lua_script_ptr->index_of_next_frame_callback_to_execute, true);
}

void RunOnGCControllerPolledCallbacks_impl(void* base_script_context_ptr)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->gc_controller_input_polled_callback_lua_thread, lua_script_ptr->gc_controller_input_polled_callback_locations, next_callback, false);
}

void RunOnWiiInputPolledCallbacks_impl(void* base_script_context_ptr)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->wii_input_polled_callback_lua_thread, lua_script_ptr->wii_controller_input_polled_callback_locations, next_callback, false);
}

void RunButtonCallbacksInQueue_impl(void* base_script_context_ptr)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  std::vector<int> vector_of_functions_to_run;
  while (!lua_script_ptr->button_callbacks_to_run.empty())
  {
    int function_reference = lua_script_ptr->button_callbacks_to_run.front();
    lua_script_ptr->button_callbacks_to_run.pop();
    vector_of_functions_to_run.push_back(function_reference);
  }
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->button_callback_thread, vector_of_functions_to_run, next_callback, false);
}

void RunOnInstructionReachedCallbacks_impl(void* base_script_context_ptr, unsigned int address)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  if (lua_script_ptr->map_of_instruction_address_to_lua_callback_locations.count(address) == 0)
    return;
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->instruction_address_hit_callback_lua_thread, lua_script_ptr->map_of_instruction_address_to_lua_callback_locations[address], next_callback, false);
}

void RunOnMemoryAddressReadFromCallbacks_impl(void* base_script_context_ptr, unsigned int address)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  std::vector<int> vector_of_functions_to_run;
  size_t number_of_callbacks = lua_script_ptr->memory_address_read_from_callback_list.size();
  for (size_t i = 0; i < number_of_callbacks; ++i)
  {
    if (address >= lua_script_ptr->memory_address_read_from_callback_list[i].memory_start_address &&
      address <= lua_script_ptr->memory_address_read_from_callback_list[i].memory_end_address)
      vector_of_functions_to_run.push_back(lua_script_ptr->memory_address_read_from_callback_list[i].callback_ref);
  }
  if (vector_of_functions_to_run.size() == 0)
    return;
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->memory_address_read_from_callback_lua_thread, vector_of_functions_to_run, next_callback, false);
}

void RunOnMemoryAddressWrittenToCallbacks_impl(void* base_script_context_ptr, unsigned int address)
{
  if (callEndIfNeededAndReturnTrueIfShouldEnd(base_script_context_ptr))
    return;
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int next_callback = 0;
  std::vector<int> vector_of_functions_to_run;
  size_t number_of_callbacks = lua_script_ptr->memory_address_written_to_callback_list.size();
  for (size_t i = 0; i < number_of_callbacks; ++i)
  {
    if (address >= lua_script_ptr->memory_address_written_to_callback_list[i].memory_start_address &&
        address <= lua_script_ptr->memory_address_written_to_callback_list[i].memory_end_address)
          vector_of_functions_to_run.push_back(lua_script_ptr->memory_address_written_to_callback_list[i].callback_ref);
  }
  if (vector_of_functions_to_run.size() == 0)
    return;
  GenericRunCallbacksHelperFunction(base_script_context_ptr, lua_script_ptr->memory_address_written_to_callback_lua_thread, vector_of_functions_to_run, next_callback, false);
}

void* RegisterForVectorHelper(std::vector<int>& input_vector, void* callback, std::atomic<size_t>& number_of_auto_deregister_callbacks)
{
  int function_reference = *((int*)(&callback));
  input_vector.push_back(function_reference);
  number_of_auto_deregister_callbacks++;
  return *((void**)(&callback));
}

bool UnregisterForVectorHelper(std::vector<int>& input_vector, void* callback)
{
  int function_reference_to_delete = *((int*)(&callback));
  if (std::find(input_vector.begin(), input_vector.end(), function_reference_to_delete) ==
    input_vector.end())
    return false;

  input_vector.erase(
    (std::find(input_vector.begin(), input_vector.end(), function_reference_to_delete)));
  return true;
}

void* RegisterForMapHelper(
  unsigned int address, std::unordered_map<unsigned int, std::vector<int> >& input_map, void* callback,
  std::atomic<size_t>& number_of_auto_deregistration_callbacks)
{
  int function_reference = *((int*)(&callback));
  if (!input_map.count(address))
    input_map[address] = std::vector<int>();
  input_map[address].push_back(function_reference);
  number_of_auto_deregistration_callbacks++;
  return *((void**)(&callback));
}

bool UnregisterForMapHelper(std::unordered_map<unsigned int, std::vector<int> >& input_map, void* callback)
{
  int function_reference_to_delete = *((int*)(&callback));
  signed long long address = -1;
  for (auto& map_iterator : input_map)
  {
    if (!map_iterator.second.empty())
    {
      // This is the case where the vector contains the callback we want to delete.
      if (std::count(map_iterator.second.begin(), map_iterator.second.end(), function_reference_to_delete) > 0)
      {
        address = map_iterator.first;
        break;
      }
    }
  }

  if (address == -1)
    return false;

  input_map[address].erase(std::find(input_map[address].begin(), input_map[address].end(), function_reference_to_delete));
  if (input_map[address].empty())
    input_map.erase(address);

  return true;
}

void* RegisterForVectorHelper(std::vector<MemoryAddressCallbackTriple>& input_vector, unsigned int start_address,
                              unsigned int end_address, void* callback, std::atomic<size_t>& number_of_auto_deregister_callbacks)
{
  int function_reference = *((int*)(&callback));
  MemoryAddressCallbackTriple new_callback_triple = MemoryAddressCallbackTriple();
  new_callback_triple.memory_start_address = start_address;
  new_callback_triple.memory_end_address = end_address;
  new_callback_triple.callback_ref = function_reference;
  input_vector.push_back(new_callback_triple);
  ++number_of_auto_deregister_callbacks;
  return callback;
}

bool UnregisterForVectorHelper(std::vector<MemoryAddressCallbackTriple>& input_vector, void* callback)
{
  int function_reference = *((int*)(&callback));
  unsigned long long vector_length = input_vector.size();
  for (unsigned long long i = 0; i < vector_length; ++i)
  {
    if (input_vector[i].callback_ref == function_reference)
    {
      input_vector.erase(input_vector.begin() + i);
      return true;
    }
  }
  return false;
}

void* RegisterOnFrameStartCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  std::atomic<size_t> garbage_val = 0;
  return RegisterForVectorHelper(lua_script_ptr->frame_callback_locations, callback, garbage_val);
}

void RegisterOnFrameStartWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForVectorHelper(lua_script_ptr->frame_callback_locations, callback,
    lua_script_ptr->number_of_frame_callbacks_to_auto_deregister);
}

int UnregisterOnFrameStartCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->frame_callback_locations, callback);
}

void* RegisterOnGCControllerPolledCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  std::atomic<size_t> garbage_val = 0;
  return RegisterForVectorHelper(lua_script_ptr->gc_controller_input_polled_callback_locations, callback, garbage_val);
}

void RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForVectorHelper(lua_script_ptr->gc_controller_input_polled_callback_locations, callback, lua_script_ptr->number_of_gc_controller_input_callbacks_to_auto_deregister);
}

int UnregisterOnGCControllerPolledCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->gc_controller_input_polled_callback_locations, callback);
}

void* RegisterOnWiiInputPolledCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  std::atomic<size_t> garbage_val = 0;
  return RegisterForVectorHelper(lua_script_ptr->wii_controller_input_polled_callback_locations, callback, garbage_val);
}

void RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForVectorHelper(lua_script_ptr->wii_controller_input_polled_callback_locations, callback, lua_script_ptr->number_of_wii_input_callbacks_to_auto_deregister);
}

int UnregisterOnWiiInputPolledCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->wii_controller_input_polled_callback_locations, callback);
}

void RegisterButtonCallback_impl(void* base_script_context_ptr, long long button_id, void* callbacks)
{
  int function_reference = *((int*)(&callbacks));
  if (function_reference >= 0)
    getLuaScriptContext(base_script_context_ptr)->map_of_button_id_to_callback[button_id] = function_reference;
}

int IsButtonRegistered_impl(void* base_script_context_ptr, long long button_id)
{
  return getLuaScriptContext(base_script_context_ptr)->map_of_button_id_to_callback.count(button_id) > 0;
}

void GetButtonCallbackAndAddToQueue_impl(void* base_script_context_ptr, long long button_id)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  int callback = lua_script_ptr->map_of_button_id_to_callback[button_id];
  lua_script_ptr->button_callbacks_to_run.push(callback);
}

void* RegisterOnInstructionReachedCallback_impl(void* base_script_context_ptr, unsigned int address, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  std::atomic<size_t> garbage_val = 0;
  return RegisterForMapHelper(address, lua_script_ptr->map_of_instruction_address_to_lua_callback_locations, callback, garbage_val);
}

void RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int address, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForMapHelper(address, lua_script_ptr->map_of_instruction_address_to_lua_callback_locations, callback, lua_script_ptr->number_of_instruction_address_callbacks_to_auto_deregister);
}

int UnregisterOnInstructionReachedCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForMapHelper(getLuaScriptContext(base_script_context_ptr)->map_of_instruction_address_to_lua_callback_locations, callback);
}

void* RegisterOnMemoryAddressReadFromCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  std::atomic<size_t> garbage_val = 0;
  return RegisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->memory_address_read_from_callback_list, start_address, end_address, callback, garbage_val);
}

void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForVectorHelper(lua_script_ptr->memory_address_read_from_callback_list, start_address, end_address, callback, lua_script_ptr->number_of_memory_address_read_callbacks_to_auto_deregister);
}

int UnregisterOnMemoryAddressReadFromCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->memory_address_read_from_callback_list, callback);
}

void* RegisterOnMemoryAddressWrittenToCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  std::atomic<size_t> garbage_val = 0;
  return RegisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->memory_address_written_to_callback_list, start_address, end_address, callback, garbage_val);
}

void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  LuaScriptContext* lua_script_ptr = getLuaScriptContext(base_script_context_ptr);
  RegisterForVectorHelper(lua_script_ptr->memory_address_written_to_callback_list, start_address, end_address, callback, lua_script_ptr->number_of_memory_address_write_callbacks_to_auto_deregister);
}

int UnregisterOnMemoryAddressWrittenToCallback_impl(void* base_script_context_ptr, void* callback)
{
  return UnregisterForVectorHelper(getLuaScriptContext(base_script_context_ptr)->memory_address_written_to_callback_list, callback);
}

void DLLClassMetadataCopyHook_impl(void* base_script_context_ptr, void* class_metadata_ptr)
{
  std::string class_name = std::string(classMetadata_APIs.GetClassName(class_metadata_ptr));
  std::vector<FunctionMetadata> functions_list = std::vector<FunctionMetadata>();
  unsigned long long number_of_functions = classMetadata_APIs.GetNumberOfFunctions(class_metadata_ptr);
  for (unsigned long long i = 0; i < number_of_functions; ++i)
  {
    void* function_metadata_ptr = classMetadata_APIs.GetFunctionMetadataAtPosition(class_metadata_ptr, i);
    FunctionMetadata current_function = FunctionMetadata();
    std::string function_name = std::string(functionMetadata_APIs.GetFunctionName(function_metadata_ptr));
    std::string function_version = std::string(functionMetadata_APIs.GetFunctionVersion(function_metadata_ptr));
    std::string example_function_call = std::string(functionMetadata_APIs.GetExampleFunctionCall(function_metadata_ptr));
    void* (*wrapped_function_ptr)(void*, void*) = functionMetadata_APIs.GetFunctionPointer(function_metadata_ptr);
    ScriptingEnums::ArgTypeEnum return_type = (ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetReturnType(function_metadata_ptr);
    unsigned long long num_args = functionMetadata_APIs.GetNumberOfArguments(function_metadata_ptr);
    std::vector<ScriptingEnums::ArgTypeEnum> argument_type_list = std::vector<ScriptingEnums::ArgTypeEnum>();
    for (unsigned long long arg_index = 0; arg_index < num_args; ++arg_index)
      argument_type_list.push_back((ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetTypeOfArgumentAtIndex(function_metadata_ptr, arg_index));
    current_function = FunctionMetadata(function_name.c_str(), function_version.c_str(), example_function_call.c_str(), wrapped_function_ptr, return_type, argument_type_list);
    functions_list.push_back(current_function);
  }

  ((LuaScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr))->class_metadata_buffer = ClassMetadata(class_name, functions_list);
}

void DLLFunctionMetadataCopyHook_impl(void* base_script_context_ptr, void* function_metadata_ptr)
{
  std::string function_name = std::string(functionMetadata_APIs.GetFunctionName(function_metadata_ptr));
  std::string function_version = std::string(functionMetadata_APIs.GetFunctionVersion(function_metadata_ptr));
  std::string example_function_call = std::string(functionMetadata_APIs.GetExampleFunctionCall(function_metadata_ptr));
  void* (*wrapped_function_ptr)(void*, void*) = functionMetadata_APIs.GetFunctionPointer(function_metadata_ptr);
  ScriptingEnums::ArgTypeEnum return_type = (ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetReturnType(function_metadata_ptr);
  unsigned long long num_args = functionMetadata_APIs.GetNumberOfArguments(function_metadata_ptr);
  std::vector<ScriptingEnums::ArgTypeEnum> argument_type_list = std::vector<ScriptingEnums::ArgTypeEnum>();
  for (unsigned long long arg_index = 0; arg_index < num_args; ++arg_index)
    argument_type_list.push_back((ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetTypeOfArgumentAtIndex(function_metadata_ptr, arg_index));
  ((LuaScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr))->single_function_metadata_buffer =
    FunctionMetadata(function_name.c_str(), function_version.c_str(), example_function_call.c_str(), wrapped_function_ptr, return_type, argument_type_list);
}
