#include "PythonScriptContext.h"
#include "../HelperFiles/PythonInterface.h"
#include "../CopiedScriptContextFiles/Enums/ScriptReturnCodes.h"
#include <vector>


void* original_python_thread = nullptr;
static std::vector<int> digital_buttons_list = std::vector<int>();
static std::vector<int> analog_buttons_list = std::vector<int>();

// Creates + returns a new PythonScriptContext object, with all fields properly initialized from the base ScriptContext* passed into the function.
void* Init_PythonScriptContext_impl(void* new_base_script_context_ptr)
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

  PythonScriptContext* new_python_script = new PythonScriptContext(new_base_script_context_ptr);
  dolphinDefinedScriptContext_APIs.set_derived_script_context_ptr(new_base_script_context_ptr, new_python_script);
  if (original_python_thread == nullptr) // This means it's our first time initializing the Python runtime.
  {
    PythonInterface::CreateThisModule();

    const void* default_module_names = moduleLists_APIs.GetListOfDefaultModules();
    unsigned long long number_of_default_modules = moduleLists_APIs.GetSizeOfList(default_module_names);
    for (unsigned long long i = 0; i < number_of_default_modules; ++i)
      PythonInterface::CreateEmptyModule(moduleLists_APIs.GetElementAtListIndex(default_module_names, i));

    const void* non_default_module_names = moduleLists_APIs.GetListOfNonDefaultModules();
    unsigned long long number_of_non_default_modules = moduleLists_APIs.GetSizeOfList(non_default_module_names);
    for (unsigned long long i = 0; i < number_of_non_default_modules; ++i)
      PythonInterface::CreateEmptyModule(moduleLists_APIs.GetElementAtListIndex(non_default_module_names, i));

    PythonInterface::Python_Initialize();
    original_python_thread = PythonInterface::PythonThreadState_Get();
  }
  else
    PythonInterface::PythonEval_RestoreThread(original_python_thread);

  new_python_script->main_python_thread = PythonInterface::Python_NewInterpreter();
  PythonInterface::SetThisModule(new_python_script->base_script_context_ptr);

  const void* default_module_names = moduleLists_APIs.GetListOfDefaultModules();
  unsigned long long number_of_default_modules = moduleLists_APIs.GetSizeOfList(default_module_names);
  const char* script_version = dolphinDefinedScriptContext_APIs.get_script_version();
  for (unsigned long long i = 0; i < number_of_default_modules; ++i)
  {
    PythonInterface::SetModuleVersion(moduleLists_APIs.GetElementAtListIndex(default_module_names, i), script_version);
    classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(new_python_script->base_script_context_ptr, moduleLists_APIs.GetElementAtListIndex(default_module_names, i), script_version);
    ClassMetadata* class_metadata_copy = new ClassMetadata(new_python_script->class_metadata_buffer);
    new_python_script->classes_to_delete.push_back(class_metadata_copy);
    PythonInterface::SetModuleFunctions(moduleLists_APIs.GetElementAtListIndex(default_module_names, i), class_metadata_copy, &new_python_script->py_method_defs_to_delete);
    PythonInterface::RunImportCommand(moduleLists_APIs.GetElementAtListIndex(default_module_names, i));
  }

  const void* non_default_module_names = moduleLists_APIs.GetListOfNonDefaultModules();
  unsigned long long number_of_non_default_modules = moduleLists_APIs.GetSizeOfList(non_default_module_names);
  for (unsigned long long i = 0; i < number_of_non_default_modules; ++i)
    PythonInterface::SetModuleVersion(moduleLists_APIs.GetElementAtListIndex(non_default_module_names, i), "0.0");


  std::string user_path = std::string(fileUtility_APIs.GetUserPath()) + "PythonLibs";
  std::replace(user_path.begin(), user_path.end(), '\\', '/');

  std::string system_path = std::string(fileUtility_APIs.GetSystemDirectory()) + "PythonLibs";
  std::replace(system_path.begin(), system_path.end(), '\\', '/');

  PythonInterface::AppendArgumentsToPath(user_path.c_str(), system_path.c_str());
  PythonInterface::RedirectStdOutAndStdErr();

  PythonInterface::PythonEval_ReleaseThread(new_python_script->main_python_thread);
  dolphinDefinedScriptContext_APIs.add_script_to_queue_of_scripts_waiting_to_start(new_python_script->base_script_context_ptr);
  return reinterpret_cast<void*>(new_python_script);
}

PythonScriptContext* GetPythonScriptContext(void* base_script_context_ptr)
{
  return reinterpret_cast<PythonScriptContext*>(dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr));
}

void unref_vector(std::vector<IdentifierToCallback>& input_vector)
{
  for (auto& identifier_callback_pair : input_vector)
    PythonInterface::Python_DecRef(identifier_callback_pair.callback);
  input_vector.clear();
}

void unref_map(std::unordered_map<unsigned int, std::vector<IdentifierToCallback>>& input_map)
{
  for (auto& addr_input_vector_pair : input_map)
    unref_vector(addr_input_vector_pair.second);
  input_map.clear();
}

void unref_vector(std::vector<MemoryAddressCallbackTriple>& input_vector)
{
  for (auto& memory_address_triple : input_vector)
    PythonInterface::Python_DecRef(memory_address_triple.identifier_to_callback.callback);
  input_vector.clear();
}

void unref_map(std::unordered_map<long long, IdentifierToCallback>& input_map)
{
  for (auto& identifier_callback_pair : input_map)
    PythonInterface::Python_DecRef(identifier_callback_pair.second.callback);
  input_map.clear();
}

void Destroy_PythonScriptContext_impl(void* base_script_context_ptr) // Takes as input a ScriptContext*, and frees the associated PythonScriptContext*
{
  PythonScriptContext* current_script = GetPythonScriptContext(base_script_context_ptr);
  if (current_script == nullptr)
    return;
  unsigned long long num_classes = current_script->classes_to_delete.size();
  for (unsigned long long i = 0; i < num_classes; ++i)
    delete ((ClassMetadata*)current_script->classes_to_delete[i]);
  current_script->classes_to_delete.clear();
  PythonInterface::DeleteMethodDefsVector(&(current_script->py_method_defs_to_delete));

  unref_vector(current_script->frame_callbacks);
  unref_vector(current_script->gc_controller_input_polled_callbacks);
  unref_vector(current_script->wii_controller_input_polled_callbacks);
  unref_map(current_script->map_of_instruction_address_to_python_callbacks);
  unref_vector(current_script->memory_address_read_from_callbacks);
  unref_vector(current_script->memory_address_written_to_callbacks);
  unref_map(current_script->map_of_button_id_to_callback);

  delete current_script;

  dolphinDefinedScriptContext_APIs.set_derived_script_context_ptr(base_script_context_ptr, nullptr);
}

void RunEndOfIteraionTasks(void* base_script_context_ptr)
{
  if (PythonInterface::Python_ErrOccured())
  {
    PythonInterface::Python_CallPyErrPrintEx();
  }

  PythonInterface::Python_SendOutputToCallbackLocationAndClear(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_print_callback_function(base_script_context_ptr));

  if (ShouldCallEndScriptFunction(base_script_context_ptr))
    dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
}

int getNumberOfCallbacksInMap(std::unordered_map<unsigned int, std::vector<IdentifierToCallback>>& input_map)
{
  int return_val = 0;
  for (auto& element : input_map)
  {
    return_val += (int)element.second.size();
  }
  return return_val;
}

bool ShouldCallEndScriptFunction(void* base_script_context_ptr)
{
  PythonScriptContext* current_script = GetPythonScriptContext(base_script_context_ptr);

  if (dolphinDefinedScriptContext_APIs.get_is_finished_with_global_code(base_script_context_ptr) &&
    (current_script->frame_callbacks.size() == 0 || (current_script->frame_callbacks.size() - current_script->number_of_frame_callbacks_to_auto_deregister <= 0)) &&
    (current_script->gc_controller_input_polled_callbacks.size() == 0 || (current_script->gc_controller_input_polled_callbacks.size() - current_script->number_of_gc_controller_input_callbacks_to_auto_deregister <= 0)) &&
    (current_script->wii_controller_input_polled_callbacks.size() == 0 || (current_script->wii_controller_input_polled_callbacks.size() - current_script->number_of_wii_input_callbacks_to_auto_deregister <= 0)) &&
    (current_script->map_of_instruction_address_to_python_callbacks.size() == 0 || (getNumberOfCallbacksInMap(current_script->map_of_instruction_address_to_python_callbacks) - current_script->number_of_instruction_address_callbacks_to_auto_deregister <= 0)) &&
    (current_script->memory_address_read_from_callbacks.size() == 0 || (current_script->memory_address_read_from_callbacks.size() - current_script->number_of_memory_address_read_callbacks_to_auto_deregister <= 0)) &&
    (current_script->memory_address_written_to_callbacks.size() == 0 || (current_script->memory_address_written_to_callbacks.size() - current_script->number_of_memory_address_write_callbacks_to_auto_deregister <= 0)) &&
    current_script->button_callbacks_to_run.empty())
    return true;
  return false;
}


// See the declarations in the DLL_Defined_ScriptContext_APIs struct in the ScriptContext_APIs.h file for documentation on what these functions do.
// Note that each of these functions takes as input for its 1st parameter an opaque void* handle to a ScriptContext* (in order to be callable from the Dolphin side).
// However, you can use the get_derived_script_context_class_ptr() method on the ScriptContext* in order to get the PythonScriptContext* associated with the ScriptContext* object.

// This function is called when a class is imported. It sets the version of the class module, sets the class module to have the right functions in it, and then runs the "import module" command.
void ImportModule_impl(void* base_script_context_ptr, const char* api_name, const char* api_version)
{
  PythonInterface::SetModuleVersion(api_name, api_version);
  classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(base_script_context_ptr, api_name, api_version);
  ClassMetadata* new_class_metadata = new ClassMetadata(((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr))->class_metadata_buffer);
  GetPythonScriptContext(base_script_context_ptr)->classes_to_delete.push_back(new_class_metadata); // storing the pointer so it can be deleted later on when the Python DLL shuts down.
  PythonInterface::SetModuleFunctions(api_name, new_class_metadata, &(GetPythonScriptContext(base_script_context_ptr)->py_method_defs_to_delete));
  PythonInterface::RunImportCommand(api_name);
}

void* HandleErrorInFunction(void* base_script_context_ptr, FunctionMetadata* function_metadata, bool include_example, const std::string& base_error_msg)
{
  std::string error_msg = "";

  if (include_example)
    error_msg = std::string("Error: In ") + function_metadata->module_name + "." + function_metadata->function_name + "() function, " + base_error_msg + ". The method should be called like this: " + function_metadata->module_name + "." + function_metadata->example_function_call;
  else
    error_msg = std::string("Error: In ") + function_metadata->module_name + "." + function_metadata->function_name + "() function, " + base_error_msg;

  PythonInterface::Python_SetRunTimeError(error_msg.c_str());
  dolphinDefinedScriptContext_APIs.set_script_return_code(base_script_context_ptr, ScriptingEnums::ScriptReturnCodes::UnknownError);
  dolphinDefinedScriptContext_APIs.set_error_message(base_script_context_ptr, error_msg.c_str());
  dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
  return nullptr;
}

void* HandleErrorGeneral(void* base_script_context_ptr, const std::string& error_msg)
{
  PythonInterface::Python_SetRunTimeError(error_msg.c_str());
  dolphinDefinedScriptContext_APIs.set_script_return_code(base_script_context_ptr, ScriptingEnums::ScriptReturnCodes::UnknownError);
  dolphinDefinedScriptContext_APIs.set_error_message(base_script_context_ptr, error_msg.c_str());
  dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
  return nullptr;

}

void* FreeAndReturn(void* arg_holder, void* ret_val)
{
  argHolder_APIs.Delete_ArgHolder(arg_holder);
  return ret_val;
}

void* RunFunction_impl(void* base_script_context_ptr, FunctionMetadata* current_function_metadata, void* self_raw, void* args_raw)
{
  const char* module_version = PythonInterface::GetModuleVersion(current_function_metadata->module_name.c_str());
  if (module_version[0] == '\0' || module_version[0] == '0')
  {
    HandleErrorGeneral(base_script_context_ptr, std::string("Error: ") + current_function_metadata->module_name + " was used without being imported! Please import the method like this: " + moduleLists_APIs.GetImportModuleName() + ".importModule(\"" + current_function_metadata->function_name + "\", \"1.0\")");
    return nullptr;
  }

  unsigned long long actual_number_of_arguments = PythonInterface::PythonTuple_GetSize(args_raw);
  unsigned long long expected_number_of_arguments = current_function_metadata->arguments_list.size();

  if (actual_number_of_arguments != expected_number_of_arguments)
  {
    return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, std::string("expected ") + std::to_string(expected_number_of_arguments) + " arguments, but got " + std::to_string(actual_number_of_arguments) + "instead");
  }

  void* vector_of_args = vectorOfArgHolder_APIs.CreateNewVectorOfArgHolders();
  void* next_arg_holder = nullptr;
  signed long long dict_index = 0;
  void* key_ref = nullptr;
  void* value_ref = nullptr;
  signed long long key_s64 = 0;
  signed long long value_s64 = 0;
  const char* button_name = "";
  bool digital_button_value = false;
  unsigned long long analog_button_value = 0;
  int button_name_as_enum = 0;
  unsigned long long container_size = 0;
  void* return_value = nullptr;
  std::string error_msg = "";

  unsigned long long temp_u64 = 0;
  signed long long temp_s64 = 0;
  float temp_float = 0.0f;
  double temp_double = 0.0f;
  const char* temp_const_char_ptr = "";
  void* return_dict = nullptr;
  void* base_iterator_ptr = nullptr;
  void* travel_iterator_ptr = nullptr;
  void* temp_void_ptr = nullptr;

  for (unsigned long long argument_index = 0; argument_index < expected_number_of_arguments; ++argument_index)
  {
    void* current_py_obj = PythonInterface::PythonTuple_GetItem(args_raw, argument_index);

    switch (current_function_metadata->arguments_list[argument_index])
    {
    case ScriptingEnums::ArgTypeEnum::Boolean:
      next_arg_holder = argHolder_APIs.CreateBoolArgHolder(PythonInterface::PythonObject_IsTrue(current_py_obj) ? 1 : 0);
      break;
    case ScriptingEnums::ArgTypeEnum::U8:
      next_arg_holder = argHolder_APIs.CreateU8ArgHolder(PythonInterface::PythonLongObj_AsU64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::U16:
      next_arg_holder = argHolder_APIs.CreateU16ArgHolder(PythonInterface::PythonLongObj_AsU64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::U32:
      next_arg_holder = argHolder_APIs.CreateU32ArgHolder(PythonInterface::PythonLongObj_AsU64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::U64:
      next_arg_holder = argHolder_APIs.CreateU64ArgHolder(PythonInterface::PythonLongObj_AsU64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::S8:
      next_arg_holder = argHolder_APIs.CreateS8ArgHolder(PythonInterface::PythonLongObj_AsS64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::S16:
      next_arg_holder = argHolder_APIs.CreateS16ArgHolder(PythonInterface::PythonLongObj_AsS64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::S32:
      next_arg_holder = argHolder_APIs.CreateS32ArgHolder(PythonInterface::PythonLongObj_AsS64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::S64:
      next_arg_holder = argHolder_APIs.CreateS64ArgHolder(PythonInterface::PythonLongObj_AsS64(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::Float:
      next_arg_holder = argHolder_APIs.CreateFloatArgHolder(PythonInterface::PythonFloatObj_AsDouble(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::Double:
      next_arg_holder = argHolder_APIs.CreateDoubleArgHolder(PythonInterface::PythonFloatObj_AsDouble(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::String:
      next_arg_holder = argHolder_APIs.CreateStringArgHolder(PythonInterface::PythonUnicodeObj_AsString(current_py_obj));
      break;
    case ScriptingEnums::ArgTypeEnum::AddressToByteMap:
      dict_index = 0;
      key_ref = PythonInterface::ResetAndGetRef_ToPyKey();
      value_ref = PythonInterface::ResetAndGetRef_ToPyVal();
      next_arg_holder = argHolder_APIs.CreateAddressToByteMapArgHolder();
      while (PythonInterface::PythonDict_Next(current_py_obj, &dict_index, &key_ref, &value_ref))
      {
        key_s64 = PythonInterface::PythonLongObj_AsS64(key_ref);
        value_s64 = PythonInterface::PythonLongObj_AsS64(value_ref);

        if (key_s64 < 0)
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Key in address-to-byte map was less than 0!");
        }
        else if (value_s64 < -128)
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Value in address-to-byte map was less than -128, which can't be represented in 1 byte!");
        }
        else if (value_s64 > 255)
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Value in address-to-byte map was greater than 255, which can't be represented in 1 byte!");
        }
        else
        {
          argHolder_APIs.AddPairToAddressToByteMapArgHolder(next_arg_holder, key_s64, value_s64);
        }
      }
      break;

    case ScriptingEnums::ArgTypeEnum::ControllerStateObject:
      next_arg_holder = argHolder_APIs.CreateControllerStateArgHolder();
      dict_index = 0;
      key_ref = PythonInterface::ResetAndGetRef_ToPyKey();
      value_ref = PythonInterface::ResetAndGetRef_ToPyVal();
      while (PythonInterface::PythonDict_Next(current_py_obj, &dict_index, &key_ref, &value_ref))
      {
        button_name = PythonInterface::PythonUnicodeObj_AsString(key_ref);
        if (button_name == nullptr || button_name[0] == '\0')
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Attempted to pass invalid button name into function.");
        }
        button_name_as_enum = gcButton_APIs.ParseGCButton(button_name);
        if (!gcButton_APIs.IsValidButtonEnum(button_name_as_enum))
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Invalid GameCube controller button name passed into function.");
        }

        if (gcButton_APIs.IsDigitalButton(button_name_as_enum))
        {
          digital_button_value = PythonInterface::PythonObject_IsTrue(value_ref);
          argHolder_APIs.SetControllerStateArgHolderValue(next_arg_holder, button_name_as_enum, digital_button_value);
        }
        else if (gcButton_APIs.IsAnalogButton(button_name_as_enum))
        {
          analog_button_value = PythonInterface::PythonLongObj_AsU64(value_ref);
          if (analog_button_value > 255)
          {
            vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
            argHolder_APIs.Delete_ArgHolder(next_arg_holder);
            return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Analog button value was outside the valid range of 0-255");
          }
          argHolder_APIs.SetControllerStateArgHolderValue(next_arg_holder, button_name_as_enum, analog_button_value);
        }
        else
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Invalid GameCube controller button name passed into function");
        }
      }
      break;

    case ScriptingEnums::ArgTypeEnum::ListOfPoints:
      next_arg_holder = argHolder_APIs.CreateListOfPointsArgHolder();
      container_size = PythonInterface::PythonList_Size(current_py_obj);
      for (unsigned long long i = 0; i < container_size; ++i)
      {
        float x = 0.0f;
        float y = 0.0f;
        void* next_item_in_list = PythonInterface::PythonList_GetItem(current_py_obj, i);
        if (true)// PythonInterface::PythonList_Check(next_item_in_list)) // case where the next item was a list of points.
        {
          if (PythonInterface::PythonList_Size(next_item_in_list) != 2)
          {
            vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
            argHolder_APIs.Delete_ArgHolder(next_arg_holder);
            return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "List of points contained a list which did NOT have exactly 2 points in it.");
          }
          x = PythonInterface::PythonFloatObj_AsDouble(PythonInterface::PythonList_GetItem(next_item_in_list, 0));
          y = PythonInterface::PythonFloatObj_AsDouble(PythonInterface::PythonList_GetItem(next_item_in_list, 1));
          argHolder_APIs.ListOfPointsArgHolderPushBack(next_arg_holder, x, y);
        }
        else if (true)//if (PythonInterface::PythonTuple_Check(next_item_in_list)) // case where next item was a tuple of points.
        {
          if (PythonInterface::PythonTuple_GetSize(next_item_in_list) != 2)
          {
            vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
            argHolder_APIs.Delete_ArgHolder(next_arg_holder);
            return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "List of points contained a tuple which did NOT have exactly 2 points in it.");
          }
          x = PythonInterface::PythonFloatObj_AsDouble(PythonInterface::PythonTuple_GetItem(next_item_in_list, 0));
          y = PythonInterface::PythonFloatObj_AsDouble(PythonInterface::PythonTuple_GetItem(next_item_in_list, 1));
          argHolder_APIs.ListOfPointsArgHolderPushBack(next_arg_holder, x, y);
        }
        else
        {
          vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
          argHolder_APIs.Delete_ArgHolder(next_arg_holder);
          return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "List of points contained an item which wasn't a tuple or list! List should have a format like [ [43.2, 45.2], [12.2, 41.8] ] or [ (43.2, 45.2), (12.2, 41.8) ]");
        }
      }
      break;

    case ScriptingEnums::ArgTypeEnum::RegistrationForButtonCallbackInputType:
      next_arg_holder = argHolder_APIs.CreateRegistrationForButtonCallbackInputTypeArgHolder(current_py_obj);
      break;

    case ScriptingEnums::ArgTypeEnum::RegistrationInputType:
      next_arg_holder = argHolder_APIs.CreateRegistrationInputTypeArgHolder(current_py_obj);
      break;

    case ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationInputType:
      next_arg_holder = argHolder_APIs.CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(current_py_obj);
      break;

    case ScriptingEnums::ArgTypeEnum::UnregistrationInputType:
      next_arg_holder = argHolder_APIs.CreateUnregistrationInputTypeArgHolder((void*)PythonInterface::PythonLongObj_AsU64(current_py_obj));
      break;

    default:
      vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);
      return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Function parameter type not supported yet for Python");
    }
    vectorOfArgHolder_APIs.PushBack(vector_of_args, next_arg_holder);
  }

  return_value = functionMetadata_APIs.RunFunction(current_function_metadata->function_pointer, base_script_context_ptr, vector_of_args);
  vectorOfArgHolder_APIs.Delete_VectorOfArgHolders(vector_of_args);

  if (return_value == nullptr)
  {
    return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "An unknown implementation error occured. Return value of functionw as NULL!");
  }

  if (argHolder_APIs.GetIsEmpty(return_value))
  {
    return FreeAndReturn(return_value, PythonInterface::GetNoneObject());
  }

  switch (((ScriptingEnums::ArgTypeEnum)argHolder_APIs.GetArgType(return_value)))
  {
  case ScriptingEnums::ArgTypeEnum::ErrorStringType:
    error_msg = std::string(argHolder_APIs.GetErrorStringFromArgHolder(return_value));
    argHolder_APIs.Delete_ArgHolder(return_value);
    return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, error_msg.c_str());

  case ScriptingEnums::ArgTypeEnum::VoidType:
    return FreeAndReturn(return_value, PythonInterface::GetNoneObject());

  case ScriptingEnums::ArgTypeEnum::YieldType:
    argHolder_APIs.Delete_ArgHolder(return_value);
    return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, false, "This is a yielding function, which can't be called in Python!");

  case ScriptingEnums::ArgTypeEnum::Boolean:
    return FreeAndReturn(return_value, argHolder_APIs.GetBoolFromArgHolder(return_value) ? PythonInterface::GetPyTrueObject() : PythonInterface::GetPyFalseObject());

  case ScriptingEnums::ArgTypeEnum::U8:
    temp_u64 = argHolder_APIs.GetU8FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("K", &temp_u64));

  case ScriptingEnums::ArgTypeEnum::U16:
    temp_u64 = argHolder_APIs.GetU16FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("K", &temp_u64));

  case ScriptingEnums::ArgTypeEnum::U32:
    temp_u64 = argHolder_APIs.GetU32FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("K", &temp_u64));

  case ScriptingEnums::ArgTypeEnum::U64:
    temp_u64 = argHolder_APIs.GetU64FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("K", &temp_u64));

  case ScriptingEnums::ArgTypeEnum::S8:
    temp_s64 = argHolder_APIs.GetS8FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("L", &temp_s64));

  case ScriptingEnums::ArgTypeEnum::S16:
    temp_s64 = argHolder_APIs.GetS16FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("L", &temp_s64));

  case ScriptingEnums::ArgTypeEnum::S32:
    temp_s64 = argHolder_APIs.GetS32FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("L", &temp_s64));

  case ScriptingEnums::ArgTypeEnum::S64:
    temp_s64 = argHolder_APIs.GetS64FromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("L", &temp_s64));

  case ScriptingEnums::ArgTypeEnum::Float:
    temp_float = argHolder_APIs.GetFloatFromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("f", &temp_float));

  case ScriptingEnums::ArgTypeEnum::Double:
    temp_double = argHolder_APIs.GetDoubleFromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("d", &temp_double));

  case ScriptingEnums::ArgTypeEnum::String:
    temp_const_char_ptr = argHolder_APIs.GetStringFromArgHolder(return_value);
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("s", &temp_const_char_ptr));

  case ScriptingEnums::ArgTypeEnum::AddressToByteMap:
    return_dict = PythonInterface::PythonDictionary_New();
    travel_iterator_ptr = base_iterator_ptr = argHolder_APIs.CreateIteratorForAddressToByteMapArgHolder(return_value);
    while (travel_iterator_ptr != nullptr)
    {
      key_s64 = argHolder_APIs.GetKeyForAddressToByteMapArgHolder(travel_iterator_ptr);
      value_s64 = argHolder_APIs.GetValueForAddressToUnsignedByteMapArgHolder(travel_iterator_ptr);

      PythonInterface::PythonDictionary_SetItem(return_dict, PythonInterface::S64_ToPythonLongObj(key_s64), PythonInterface::S64_ToPythonLongObj(value_s64));
      travel_iterator_ptr = argHolder_APIs.IncrementIteratorForAddressToByteMapArgHolder(travel_iterator_ptr, return_value);
    }
    argHolder_APIs.Delete_IteratorForAddressToByteMapArgHolder(base_iterator_ptr);
    return FreeAndReturn(return_value, return_dict);

  case ScriptingEnums::ArgTypeEnum::ControllerStateObject:
    return_dict = PythonInterface::PythonDictionary_New();
    for (int next_button : digital_buttons_list)
    {
      const char* button_name = gcButton_APIs.ConvertButtonEnumToString(next_button);
      bool button_value = argHolder_APIs.GetControllerStateArgHolderValue(return_value, next_button);
      PythonInterface::PythonDictionary_SetItem(return_dict, PythonInterface::StringTo_PythonUnicodeObj(button_name), button_value ? PythonInterface::GetPyTrueObject() : PythonInterface::GetPyFalseObject());
    }
    for (int next_button : analog_buttons_list)
    {
      const char* button_name = gcButton_APIs.ConvertButtonEnumToString(next_button);
      unsigned long long button_value = argHolder_APIs.GetControllerStateArgHolderValue(return_value, next_button);
      PythonInterface::PythonDictionary_SetItem(return_dict, PythonInterface::StringTo_PythonUnicodeObj(button_name), PythonInterface::U64_ToPythonLongObj(button_value));
    }
    return FreeAndReturn(return_value, return_dict);

  case ScriptingEnums::ArgTypeEnum::RegistrationReturnType:
    if (PythonInterface::Python_ErrOccured())
    {
      argHolder_APIs.Delete_ArgHolder(return_value);
      return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Unknown implementation error occured involving registration retun type");
    }
    temp_void_ptr = argHolder_APIs.GetVoidPointerFromArgHolder(return_value);
    temp_u64 = *(reinterpret_cast<unsigned long long*>(&temp_void_ptr));
    return FreeAndReturn(return_value, PythonInterface::Python_BuildValue("K", &temp_u64));

  case ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType:
  case ScriptingEnums::ArgTypeEnum::UnregistrationReturnType:
    return FreeAndReturn(return_value, PythonInterface::GetNoneObject());

  case ScriptingEnums::ArgTypeEnum::ShutdownType:
    argHolder_APIs.Delete_ArgHolder(return_value);
    dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
    return PythonInterface::GetNoneObject();

  default:
    argHolder_APIs.Delete_ArgHolder(return_value);
    return HandleErrorInFunction(base_script_context_ptr, current_function_metadata, true, "Unsupported return type encountered in python function (this type has not been implemented as a return type yet.");
  }
}

void StartScript_impl(void* base_script_context_ptr)
{
  if (!dolphinDefinedScriptContext_APIs.get_is_script_active(base_script_context_ptr))
    return;
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  PythonInterface::PythonEval_RestoreThread(python_script->main_python_thread);
  PythonInterface::Python_RunFile(dolphinDefinedScriptContext_APIs.get_script_filename(base_script_context_ptr));
  dolphinDefinedScriptContext_APIs.set_is_finished_with_global_code(base_script_context_ptr, 1); // Since Python can only run global code when it first starts a script, we always set this to true after we finish executing the file for the 1st time.
  RunEndOfIteraionTasks(base_script_context_ptr);
  PythonInterface::PythonEval_ReleaseThread(python_script->main_python_thread);
}


void RunGlobalScopeCode_impl(void* base_script_context_ptr)
{
  return; // Python can only run global code when it starts a script for the 1st time (in StartScript_impl()). Thus, this function does nothing.
}

void RunCallbacksForVector(void* base_script_context_ptr, PythonScriptContext* python_script, std::vector<IdentifierToCallback>& callback_list)
{
  if (!dolphinDefinedScriptContext_APIs.get_is_script_active(base_script_context_ptr))
    return;

  if (ShouldCallEndScriptFunction(base_script_context_ptr))
  {
    dolphinDefinedScriptContext_APIs.get_script_end_callback_function(base_script_context_ptr)(base_script_context_ptr, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(base_script_context_ptr));
    return;
  }

  if (callback_list.size() == 0)
    return;

  PythonInterface::PythonEval_RestoreThread(python_script->main_python_thread);

  for (unsigned long long i = 0; i < callback_list.size(); ++i)
  {
    if (callback_list[i].callback == nullptr)
      break;
    PythonInterface::PythonObject_CallFunction(callback_list[i].callback);
  }
  RunEndOfIteraionTasks(base_script_context_ptr);
  PythonInterface::PythonEval_ReleaseThread(python_script->main_python_thread);
}

void RunOnFrameStartCallbacks_impl(void* base_script_context_ptr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RunCallbacksForVector(base_script_context_ptr, python_script, python_script->frame_callbacks);
}

void RunOnGCControllerPolledCallbacks_impl(void* base_script_context_ptr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RunCallbacksForVector(base_script_context_ptr, python_script, python_script->gc_controller_input_polled_callbacks);
}

void RunOnWiiInputPolledCallbacks_impl(void* base_script_context_ptr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RunCallbacksForVector(base_script_context_ptr, python_script, python_script->wii_controller_input_polled_callbacks);
}

void RunButtonCallbacksInQueue_impl(void* base_script_context_ptr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  std::vector<IdentifierToCallback> vector_of_callbacks = {};
  while (!python_script->button_callbacks_to_run.empty())
  {
    vector_of_callbacks.push_back(python_script->button_callbacks_to_run.front());
    python_script->button_callbacks_to_run.pop();
  }
  RunCallbacksForVector(base_script_context_ptr, python_script, vector_of_callbacks);
}

void RunOnInstructionReachedCallbacks_impl(void* base_script_context_ptr, unsigned int addr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  if (python_script->map_of_instruction_address_to_python_callbacks.count(addr) == 0)
    return;
  RunCallbacksForVector(base_script_context_ptr, python_script, python_script->map_of_instruction_address_to_python_callbacks[addr]);
}

void RunOnMemoryAddressReadFromCallbacks_impl(void* base_script_context_ptr, unsigned int address)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  std::vector<IdentifierToCallback> callbacks_to_run = std::vector<IdentifierToCallback>();
  size_t list_size = python_script->memory_address_read_from_callbacks.size();
  for (size_t i = 0; i < list_size; ++i)
  {
    if (address >= python_script->memory_address_read_from_callbacks[i].memory_start_address &&
        address <= python_script->memory_address_read_from_callbacks[i].memory_end_address)
          callbacks_to_run.push_back(python_script->memory_address_read_from_callbacks[i].identifier_to_callback);
  }

  if (callbacks_to_run.size() == 0)
    return;
  RunCallbacksForVector(base_script_context_ptr, python_script, callbacks_to_run);
}

void RunOnMemoryAddressWrittenToCallbacks_impl(void* base_script_context_ptr, unsigned int address)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  std::vector<IdentifierToCallback> callbacks_to_run = std::vector<IdentifierToCallback>();
  size_t list_size = python_script->memory_address_written_to_callbacks.size();
  for (size_t i = 0; i < list_size; ++i)
  {
    if (address >= python_script->memory_address_written_to_callbacks[i].memory_start_address &&
        address <= python_script->memory_address_written_to_callbacks[i].memory_end_address)
          callbacks_to_run.push_back(python_script->memory_address_written_to_callbacks[i].identifier_to_callback);
  }

  if (callbacks_to_run.size() == 0)
    return;
  RunCallbacksForVector(base_script_context_ptr, python_script, callbacks_to_run);
}

void* RegisterForVectorHelper(PythonScriptContext* python_script, std::vector<IdentifierToCallback>& callback_list, void* callback)
{
  size_t return_result = 0;
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(python_script->base_script_context_ptr, "Error: Attempted to register a Python object which wasn't callable as a callback function!");
    return nullptr;
  }
  PythonInterface::Python_IncRef(callback);
  callback_list.push_back({ python_script->next_unique_identifier_for_callback, callback });
  return_result = python_script->next_unique_identifier_for_callback;
  python_script->next_unique_identifier_for_callback++;
  return *((void**)(&return_result));
}

void RegisterForVectorWithAutoDeregistrationHelper(void* base_script_context_ptr, std::vector<IdentifierToCallback>& callback_list, void* callback, std::atomic<size_t>& number_of_callbacks_to_auto_deregister)
{
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: attempted to register as an auto-deregister function a python object which wasn't callable!");
    return;
  }
  PythonInterface::Python_IncRef(callback);
  callback_list.push_back({ 0, callback });
  ++number_of_callbacks_to_auto_deregister;
}

bool UnregisterForVectorHelper(void* base_script_context_ptr, std::vector<IdentifierToCallback>& callback_list, void* identifier_for_callback)
{
  size_t casted_callback_identifier = *((size_t*)&identifier_for_callback);
  if (casted_callback_identifier == 0)
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to de-register a callback which was registered with auto-deregistration. These are deleted by the app automatically when there's no more non-auto-deregistered callbacks left to run");
    return false;
  }
  size_t list_size = callback_list.size();

  for (size_t i = 0; i < list_size; ++i)
  {
    if (callback_list[i].identifier == casted_callback_identifier)
    {
      PythonInterface::Python_DecRef(callback_list[i].callback);
      callback_list.erase(callback_list.begin() + i);
      return true;
    }
  }

  HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to unregister a function which wasn't currently registered!");
  return false;
}

void* RegisterOnFrameStartCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForVectorHelper(python_script, python_script->frame_callbacks, callback_func);
}

void RegisterOnFrameStartWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForVectorWithAutoDeregistrationHelper(base_script_context_ptr, python_script->frame_callbacks, callback_func, python_script->number_of_frame_callbacks_to_auto_deregister);
}

int UnregisterOnFrameStartCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForVectorHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->frame_callbacks, identifier_for_callback);
}

void* RegisterOnGCControllerPolledCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForVectorHelper(python_script, python_script->gc_controller_input_polled_callbacks, callback_func);
}

void RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForVectorWithAutoDeregistrationHelper(base_script_context_ptr, python_script->gc_controller_input_polled_callbacks, callback_func, python_script->number_of_gc_controller_input_callbacks_to_auto_deregister);
}

int UnregisterOnGCControllerPolledCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForVectorHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->gc_controller_input_polled_callbacks, identifier_for_callback);
}

void* RegisterOnWiiInputPolledCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForVectorHelper(python_script, python_script->wii_controller_input_polled_callbacks, callback_func);
}

void RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, void* callback_func)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForVectorWithAutoDeregistrationHelper(base_script_context_ptr, python_script->wii_controller_input_polled_callbacks, callback_func, python_script->number_of_wii_input_callbacks_to_auto_deregister);
}

int UnregisterOnWiiInputPolledCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForVectorHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->wii_controller_input_polled_callbacks, identifier_for_callback);
}

void RegisterButtonCallback_impl(void* base_script_context_ptr, long long button_id, void* callback)
{
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to register a Python object which wasn't callable as a button-clicked callback function!");
    return;
  }
  PythonInterface::Python_IncRef(callback);
  GetPythonScriptContext(base_script_context_ptr)->map_of_button_id_to_callback[button_id] = { 0, callback };
}

int IsButtonRegistered_impl(void* base_script_context_ptr, long long button_id)
{
  return GetPythonScriptContext(base_script_context_ptr)->map_of_button_id_to_callback.count(button_id) != 0 ? 1 : 0;
}

void GetButtonCallbackAndAddToQueue_impl(void* base_script_context_ptr, long long button_id)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  if (!python_script->map_of_button_id_to_callback.contains(button_id))
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to get a non-existent button callback");
    return;
  }
  python_script->button_callbacks_to_run.push(python_script->map_of_button_id_to_callback[button_id]);
}

void* RegisterForMapHelper(PythonScriptContext* python_script, unsigned int addr, std::unordered_map<unsigned int, std::vector<IdentifierToCallback> >& map_of_callbacks, void* callback)
{
  size_t return_result = 0;
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(python_script->base_script_context_ptr, "Error: Attempted to register a Python object which wasn't callable as a callback function!");
    return nullptr;
  }

  if (map_of_callbacks.count(addr) == 0)
    map_of_callbacks[addr] = std::vector<IdentifierToCallback>();
  PythonInterface::Python_IncRef(callback);
  map_of_callbacks[addr].push_back({ python_script->next_unique_identifier_for_callback, callback });
  return_result = python_script->next_unique_identifier_for_callback;
  ++python_script->next_unique_identifier_for_callback;
  return *((void**)(&return_result));
}

void RegisterForMapWithAutoDeregistrationHelper(void* base_script_context_ptr, unsigned int addr, std::unordered_map<unsigned int, std::vector<IdentifierToCallback> >& map_of_callbacks, void* callback, std::atomic<size_t>& number_of_auto_deregistration_callbacks)
{
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to register with auto-deregistration a Python object which wasn't callable as a callback function!");
    return;
  }
  if (map_of_callbacks.count(addr) == 0)
    map_of_callbacks[addr] = std::vector<IdentifierToCallback>();
  PythonInterface::Python_IncRef(callback);
  map_of_callbacks[addr].push_back({ 0, callback });
  ++number_of_auto_deregistration_callbacks;
}

bool UnregisterForMapHelper(void* base_script_context_ptr, std::unordered_map<unsigned int, std::vector<IdentifierToCallback> >& map_of_callbacks, void* identifier_for_callback)
{
  size_t identifier_to_unregister = *((size_t*)(&identifier_for_callback));
  if (identifier_to_unregister == 0)
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to unregister a function which was registered with auto-deregistration enabled. These functions are unregistered automatically when the only callbacks still running are auto-deregistered callbacks.");
    return false;
  }

  unsigned long long address = -1;

  for (auto& map_iterator : map_of_callbacks)
  {
    if (!map_iterator.second.empty())
    {
      for (auto& callback_to_identifier : map_iterator.second)
      {
        if (callback_to_identifier.identifier == identifier_to_unregister)
        {
          address = map_iterator.first;
          break;
        }
      }
    }
  }

  if (address == -1)
  {
    // This function is only called from an OnInstructionHit.unregister() call.
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to unregister an OnInstructionHit callback function which wasn't currently registered!");
    return false;
  }

  size_t list_size = map_of_callbacks[address].size();
  for (unsigned i = 0; i < list_size; ++i)
  {
    if (map_of_callbacks[address][i].identifier == identifier_to_unregister)
    {
      PythonInterface::Python_DecRef(map_of_callbacks[address][i].callback);
      map_of_callbacks[address].erase(map_of_callbacks[address].begin() + i);
    }
  }

  if (map_of_callbacks[address].empty())
    map_of_callbacks.erase(address);

  return true;
}

void* RegisterOnInstructionReachedCallback_impl(void* base_script_context_ptr, unsigned int addr, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForMapHelper(python_script, addr, python_script->map_of_instruction_address_to_python_callbacks, callback);
}

void RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int addr, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForMapWithAutoDeregistrationHelper(base_script_context_ptr, addr, python_script->map_of_instruction_address_to_python_callbacks, callback, python_script->number_of_instruction_address_callbacks_to_auto_deregister);
}

int UnregisterOnInstructionReachedCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForMapHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->map_of_instruction_address_to_python_callbacks, identifier_for_callback);
}

void* RegisterForVectorHelper(PythonScriptContext* python_script, std::vector<MemoryAddressCallbackTriple>& input_vector, unsigned int start_address, unsigned int end_address, void* callback)
{
  size_t return_result = 0;
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(python_script->base_script_context_ptr, "Error: Attempted to register a Python object which wasn't callable as a callback function!");
    return nullptr;
  }

  PythonInterface::Python_IncRef(callback);
  MemoryAddressCallbackTriple new_callback_triple = {};
  new_callback_triple.memory_start_address = start_address;
  new_callback_triple.memory_end_address = end_address;
  new_callback_triple.identifier_to_callback = {python_script->next_unique_identifier_for_callback, callback};
  input_vector.push_back(new_callback_triple);
  return_result = python_script->next_unique_identifier_for_callback;
  python_script->next_unique_identifier_for_callback++;
  return *((void**)(&return_result));
}

void RegisterForVectorWithAutoDeregistrationHelper(void* base_script_context_ptr, std::vector<MemoryAddressCallbackTriple>& input_vector, unsigned int start_address, unsigned int end_address, void* callback, std::atomic<size_t>& number_of_callbacks_to_auto_deregister)
{
  if (callback == nullptr || !PythonInterface::Python_IsCallable(callback))
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: attempted to register as an auto-deregister function a python object which wasn't callable!");
    return;
  }

  PythonInterface::Python_IncRef(callback);
  MemoryAddressCallbackTriple new_callback_triple = {};
  new_callback_triple.memory_start_address = start_address;
  new_callback_triple.memory_end_address = end_address;
  new_callback_triple.identifier_to_callback = { 0, callback };
  input_vector.push_back(new_callback_triple);
  ++number_of_callbacks_to_auto_deregister;
}

bool UnregisterForVectorHelper(void* base_script_context_ptr, std::vector<MemoryAddressCallbackTriple>& callback_list, void* identifier_for_callback)
{
  size_t casted_callback_identifier = *((size_t*)&identifier_for_callback);
  if (casted_callback_identifier == 0)
  {
    HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to de-register a callback which was registered with auto-deregistration. These are deleted by the app automatically when the only callbacks that are still running are registered with auto-deregistration.");
    return false;
  }
  size_t list_size = callback_list.size();

  for (size_t i = 0; i < list_size; ++i)
  {
    if (callback_list[i].identifier_to_callback.identifier == casted_callback_identifier)
    {
      PythonInterface::Python_DecRef(callback_list[i].identifier_to_callback.callback);
      callback_list.erase(callback_list.begin() + i);
      return true;
    }
  }

  HandleErrorGeneral(base_script_context_ptr, "Error: Attempted to unregister a function which wasn't currently registered!");
  return false;
}

void* RegisterOnMemoryAddressReadFromCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForVectorHelper(python_script, python_script->memory_address_read_from_callbacks, start_address, end_address, callback);
}

void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForVectorWithAutoDeregistrationHelper(base_script_context_ptr, python_script->memory_address_read_from_callbacks, start_address, end_address, callback, python_script->number_of_memory_address_read_callbacks_to_auto_deregister);
}

int UnregisterOnMemoryAddressReadFromCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForVectorHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->memory_address_read_from_callbacks, identifier_for_callback);
}

void* RegisterOnMemoryAddressWrittenToCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  return RegisterForVectorHelper(python_script, python_script->memory_address_written_to_callbacks, start_address, end_address, callback);
}

void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void* base_script_context_ptr, unsigned int start_address, unsigned int end_address, void* callback)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  RegisterForVectorWithAutoDeregistrationHelper(base_script_context_ptr, python_script->memory_address_written_to_callbacks, start_address, end_address, callback, python_script->number_of_memory_address_write_callbacks_to_auto_deregister);
}

int UnregisterOnMemoryAddressWrittenToCallback_impl(void* base_script_context_ptr, void* identifier_for_callback)
{
  return UnregisterForVectorHelper(base_script_context_ptr, GetPythonScriptContext(base_script_context_ptr)->memory_address_written_to_callbacks, identifier_for_callback);
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
    current_function = FunctionMetadata(class_name.c_str(), function_name.c_str(), function_version.c_str(), example_function_call.c_str(), wrapped_function_ptr, return_type, argument_type_list);
    functions_list.push_back(current_function);
  }

  ((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr))->class_metadata_buffer.class_name = std::move(std::string(class_name.c_str()));
  ((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_ptr(base_script_context_ptr))->class_metadata_buffer.functions_list = std::move(functions_list);
}

// This function is unused, but it's implemented in case we ever want to get a single FunctionMetadata* for a specific function
void DLLFunctionMetadataCopyHook_impl(void* base_script_context_ptr, void* function_metadata_ptr)
{
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  python_script->single_function_metadata_buffer.module_name = std::string("");
  python_script->single_function_metadata_buffer.function_name = std::string(functionMetadata_APIs.GetFunctionName(function_metadata_ptr));
  python_script->single_function_metadata_buffer.function_version = std::string(functionMetadata_APIs.GetFunctionVersion(function_metadata_ptr));
  python_script->single_function_metadata_buffer.return_type = (ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetReturnType(function_metadata_ptr);
  python_script->single_function_metadata_buffer.example_function_call = std::string(functionMetadata_APIs.GetExampleFunctionCall(function_metadata_ptr));
  python_script->single_function_metadata_buffer.function_pointer = functionMetadata_APIs.GetFunctionPointer(function_metadata_ptr);
  std::vector<ScriptingEnums::ArgTypeEnum> new_argument_list = {};
  unsigned long long num_args = functionMetadata_APIs.GetNumberOfArguments(function_metadata_ptr);
  for (unsigned long long arg_index = 0; arg_index < num_args; ++arg_index)
    new_argument_list.push_back((ScriptingEnums::ArgTypeEnum)functionMetadata_APIs.GetTypeOfArgumentAtIndex(function_metadata_ptr, arg_index));
  python_script->single_function_metadata_buffer.arguments_list = new_argument_list;
}
