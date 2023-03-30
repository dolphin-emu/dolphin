#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

#include "Common/FileUtil.h"

#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs/OnFrameStartCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnGCControllerPolledCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnWiiInputPolledCallbackAPI.h"

#include "Core/Scripting/HelperClasses/GCButtons.h"

#include "Core/Scripting/InternalAPIModules/BitAPI.h"
#include "Core/Scripting/InternalAPIModules/EmuAPI.h"
#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/InternalAPIModules/MemoryAPI.h"
#include "Core/Scripting/InternalAPIModules/RegistersAPI.h"
#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"

#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/BitModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/EmuModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/GameCubeControllerModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/GraphicsModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/MemoryModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnFrameStartCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnGCControllerPolledCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnInstructionHitCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnMemoryAddressReadFromCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnMemoryAddressWrittenToCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/OnWiiInputPolledCallbackModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/RegistersModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/StatisticsModuleImporter.h"
#include <fstream>

namespace Scripting::Python
{
static const char* THIS_MODULE_NAME = "ThisPointerModule";
static bool python_initialized = false;
static PyThreadState* original_python_thread = nullptr;
static std::string error_buffer_str;


int getNumberOfCallbacksInMap(std::unordered_map<size_t, std::vector<PyObject*>>& input_map)
{
  int return_val = 0;
  for (auto& element : input_map)
  {
    return_val += (int)element.second.size();
  }
  return return_val;
}

bool PythonScriptContext::ShouldCallEndScriptFunction()
{
  if (finished_with_global_code &&
      (frame_callbacks.size() == 0 ||
       frame_callbacks.size() - number_of_frame_callbacks_to_auto_deregister <= 0) &&
      (gc_controller_input_polled_callbacks.size() == 0 ||
       gc_controller_input_polled_callbacks.size() -
               number_of_gc_controller_input_callbacks_to_auto_deregister <=
           0) &&
      (wii_controller_input_polled_callbacks.size() == 0 ||
       wii_controller_input_polled_callbacks.size() -
               number_of_wii_input_callbacks_to_auto_deregister <=
           0) &&
      (map_of_instruction_address_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_instruction_address_to_python_callbacks) -
               number_of_instruction_address_callbacks_to_auto_deregister <=
           0) &&
      (map_of_memory_address_read_from_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_memory_address_read_from_to_python_callbacks) -
               number_of_memory_address_read_callbacks_to_auto_deregister <=
           0) &&
      (map_of_memory_address_written_to_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_memory_address_written_to_to_python_callbacks) -
               number_of_memory_address_write_callbacks_to_auto_deregister <=
           0) && button_callbacks_to_run.IsEmpty())
    return true;
  return false;
}


PyModuleDef ThisModuleDef = {
    PyModuleDef_HEAD_INIT, THIS_MODULE_NAME, /* name of module */
    nullptr,                                 /* module documentation, may be NULL */
    sizeof(long long*),                       /* size of per-interpreter state of the module,
                                                             or -1 if the module keeps state in global variables. */
    nullptr};


PyMODINIT_FUNC PyInit_ThisPointerModule()
{
  return PyModule_Create(&ThisModuleDef);
}

void SetModuleVersion(const char* module_name, std::string new_version_number)
{
  *((std::string*)PyModule_GetState(PyImport_ImportModule(module_name))) = std::string(new_version_number);
}

void RunImportCommand(const char* module_name)
{
  PyRun_SimpleString((std::string("import ") + module_name).c_str());
}

static const char* redirect_output_module_name = "RedirectStdOut";

PythonScriptContext::PythonScriptContext(
    int new_unique_script_identifier, const std::string& new_script_filename,
                    std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                    std::function<void(const std::string&)>* new_print_callback,
                    std::function<void(int)>* new_script_end_callback)
    : ScriptContext(new_unique_script_identifier, new_script_filename,
                    new_pointer_to_list_of_all_scripts, new_print_callback, new_script_end_callback)
{
  const std::lock_guard<std::mutex> lock(script_specific_lock);
  error_buffer_str = "";
  list_of_imported_modules = std::vector<PyObject*>();
  if (!python_initialized)
  {
    PyImport_AppendInittab(THIS_MODULE_NAME, PyInit_ThisPointerModule);
    PyImport_AppendInittab(ImportAPI::class_name, ImportModuleImporter::PyInit_ImportAPI);
    PyImport_AppendInittab(OnFrameStartCallbackAPI::class_name, OnFrameStartCallbackModuleImporter::PyInit_OnFrameStart);
    PyImport_AppendInittab(OnGCControllerPolledCallbackAPI::class_name, OnGCControllerPolledCallbackModuleImporter::PyInit_OnGCControllerPolled);
    PyImport_AppendInittab(OnInstructionHitCallbackAPI::class_name, OnInstructionHitCallbackModuleImporter::PyInit_OnInstructionHit);
    PyImport_AppendInittab(OnMemoryAddressReadFromCallbackAPI::class_name, OnMemoryAddressReadFromCallbackModuleImporter::PyInit_OnMemoryAddressReadFrom);
    PyImport_AppendInittab(OnMemoryAddressWrittenToCallbackAPI::class_name, OnMemoryAddressWrittenToCallbackModuleImporter::PyInit_OnMemoryAddressWrittenTo);
    PyImport_AppendInittab(OnWiiInputPolledCallbackAPI::class_name, OnWiiInputPolledCallbackModuleImporter::PyInit_OnWiiInputPolled);
    PyImport_AppendInittab(BitApi::class_name, BitModuleImporter::PyInit_BitAPI);
    PyImport_AppendInittab(EmuApi::class_name, EmuModuleImporter::PyInit_EmuAPI);
    PyImport_AppendInittab(GameCubeControllerApi::class_name, GameCubeControllerModuleImporter::PyInit_GameCubeControllerAPI);
    PyImport_AppendInittab(GraphicsAPI::class_name, GraphicsModuleImporter::PyInit_GraphicsAPI);
    PyImport_AppendInittab(MemoryApi::class_name, MemoryModuleImporter::PyInit_MemoryAPI);
    PyImport_AppendInittab(RegistersAPI::class_name, RegistersModuleImporter::PyInit_RegistersAPI);
    PyImport_AppendInittab(StatisticsApi::class_name, StatisticsModuleImporter::PyInit_StatisticsAPI);
    Py_Initialize();
    original_python_thread = PyThreadState_Get();
    python_initialized = true;
  }
  else
  {
    PyEval_RestoreThread(original_python_thread);
  }
  main_python_thread = Py_NewInterpreter();
  long long this_address = (long long) (ScriptContext*) this;
  *((long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))) =  this_address;

  SetModuleVersion(ImportAPI::class_name, most_recent_script_version);
  RunImportCommand(ImportAPI::class_name);
  SetModuleVersion(OnFrameStartCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnFrameStartCallbackAPI::class_name);
  SetModuleVersion(OnGCControllerPolledCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnGCControllerPolledCallbackAPI::class_name);
  SetModuleVersion(OnInstructionHitCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnInstructionHitCallbackAPI::class_name);
  SetModuleVersion(OnMemoryAddressReadFromCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnMemoryAddressReadFromCallbackAPI::class_name);
  SetModuleVersion(OnMemoryAddressWrittenToCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnMemoryAddressWrittenToCallbackAPI::class_name);
  SetModuleVersion(OnWiiInputPolledCallbackAPI::class_name, most_recent_script_version);
  RunImportCommand(OnWiiInputPolledCallbackAPI::class_name);

  SetModuleVersion(BitApi::class_name, std::string("0.0"));
  SetModuleVersion(EmuApi::class_name, std::string("0.0"));
  SetModuleVersion(GameCubeControllerApi::class_name, std::string("0.0"));
  SetModuleVersion(GraphicsAPI::class_name, std::string("0.0"));
  SetModuleVersion(MemoryApi::class_name, std::string("0.0"));
  SetModuleVersion(RegistersAPI::class_name, std::string("0.0"));
  SetModuleVersion(StatisticsApi::class_name, std::string("0.0"));

  current_script_call_location = ScriptCallLocations::FromScriptStartup;
                                      // this is python code to redirect stdouts/stderr
  std::string user_path = File::GetUserPath(D_LOAD_IDX) + "PythonLibs";
  std::replace(user_path.begin(), user_path.end(), '\\', '/');

  std::string system_path = File::GetSysDirectory() + "PythonLibs";
  std::replace(system_path.begin(), system_path.end(), '\\', '/');

  PyRun_SimpleString((std::string("import sys\nsys.path.append('") + user_path + "')\n").c_str());
  PyRun_SimpleString((std::string("sys.path.append('") + system_path + "')\n").c_str());
  PyRun_SimpleString((std::string("import ") + redirect_output_module_name + "\n")
                         .c_str());  // invoke code to redirect
  PyImport_ImportModule(redirect_output_module_name);
  PyEval_ReleaseThread(main_python_thread);
  AddScriptToQueueOfScriptsWaitingToStart(this);
}

bool ExtractBoolFromObject(PyObject* py_obj)
{
  return static_cast<bool>(PyObject_IsTrue(py_obj));
}

PyObject* PythonScriptContext::RunFunction(PyObject* self, PyObject* args, std::string class_name, std::string function_name)
{
  if (class_name.empty())
  {
    return HandleError(nullptr, nullptr, false, "Class name was NULL!");
  }

  if (function_name.empty())
  {
    return HandleError(class_name.c_str(), nullptr, false, 
        "Function metadata in was NULL! This means that the functions in this class weren't properly initialized, which is probably an error in Dolphin's internal code");
   
  }

  ScriptContext* this_pointer = reinterpret_cast<ScriptContext*>(*((long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))));
  std::string version_number = *((std::string*)PyModule_GetState(PyImport_ImportModule(class_name.c_str())));

  if (version_number == "0.0")
  {
    return HandleError(
        class_name.c_str(), nullptr, false,
        "Attempted to call function of class without properly importing it. Please import "
        "the builtin classes for Dolphin like this: 'dolphin.importModule(\"EmuAPI\", \"1.0\")' "
        "(note that version number must be >= \"1.0\")");
  }
  FunctionMetadata functionMetadata = {};

  if (class_name == ImportAPI::class_name)
    functionMetadata = ImportAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnFrameStartCallbackAPI::class_name)
    functionMetadata = OnFrameStartCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnGCControllerPolledCallbackAPI::class_name)
    functionMetadata = OnGCControllerPolledCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnInstructionHitCallbackAPI::class_name)
    functionMetadata = OnInstructionHitCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnMemoryAddressReadFromCallbackAPI::class_name)
    functionMetadata = OnMemoryAddressReadFromCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnMemoryAddressWrittenToCallbackAPI::class_name)
    functionMetadata = OnMemoryAddressWrittenToCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == OnWiiInputPolledCallbackAPI::class_name)
    functionMetadata = OnWiiInputPolledCallbackAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == BitApi::class_name)
    functionMetadata = BitApi::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == EmuApi::class_name)
    functionMetadata = EmuApi::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == GameCubeControllerApi::class_name)
    functionMetadata = GameCubeControllerApi::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == MemoryApi::class_name)
    functionMetadata = MemoryApi::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == RegistersAPI::class_name)
    functionMetadata = RegistersAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == StatisticsApi::class_name)
    functionMetadata = StatisticsApi::GetFunctionMetadataForVersion(version_number, function_name);
  else
  {
    return HandleError(nullptr, nullptr, false,
                       fmt::format("Class name {} was undefined", class_name));
  }

  if (functionMetadata.function_pointer == nullptr)
  {
    return HandleError(class_name.c_str(), nullptr, false,
                       fmt::format("Function {} was undefined for class {} and version {}",
                                   function_name, class_name, version_number));
  }

  std::map<long long, u8> address_to_unsigned_byte_map;
  std::map<long long, s8> address_to_signed_byte_map;
  std::map<long long, s16> address_to_byte_map;
  Py_ssize_t pos = 0;
  PyObject *key, *value;
  PyObject* return_dictionary_object;
  Movie::ControllerState controller_state_arg = {};
  long long magnitude = 0;
  std::vector<ImVec2> list_of_points = std::vector<ImVec2>();
  Py_ssize_t list_size = 0;


  std::vector<ArgHolder> arguments_list = std::vector<ArgHolder>();

  if (PyTuple_GET_SIZE(args) != (int) functionMetadata.arguments_list.size())
  {
    return HandleError(class_name.c_str(), &functionMetadata, true, fmt::format("Expected {} arguments, but got {} arguments instead.", functionMetadata.arguments_list.size(), PyTuple_GET_SIZE(args)));
  }

  int argument_index = 0;
  for (auto argument_type : functionMetadata.arguments_list)
  {
    PyObject* current_item = PyTuple_GET_ITEM(args, argument_index);

    switch (argument_type)
    {
    case ArgTypeEnum::Boolean:
      arguments_list.push_back(CreateBoolArgHolder(static_cast<bool>(PyObject_IsTrue(current_item))));
      break;

    case ArgTypeEnum::U8:
      arguments_list.push_back(CreateU8ArgHolder(static_cast<u8>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U16:
      arguments_list.push_back(
          CreateU16ArgHolder(static_cast<u16>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U32:
      arguments_list.push_back(
          CreateU32ArgHolder(static_cast<u32>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U64:
      arguments_list.push_back(CreateU64ArgHolder(static_cast<u64>(PyLong_AsUnsignedLongLong(current_item))));
      break;

    case ArgTypeEnum::S8:
      arguments_list.push_back(CreateS8ArgHolder(static_cast<s8>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::S16:
      arguments_list.push_back(
          CreateS16ArgHolder(static_cast<s16>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::Integer:
      arguments_list.push_back(
          CreateIntArgHolder(static_cast<int>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::LongLong:
      arguments_list.push_back(CreateLongLongArgHolder(PyLong_AsLongLong(current_item)));
      break;

    case ArgTypeEnum::Float:
      arguments_list.push_back(CreateFloatArgHolder(static_cast<float>(PyFloat_AsDouble(current_item))));
      break;
    case ArgTypeEnum::Double:
      arguments_list.push_back(CreateDoubleArgHolder(PyFloat_AsDouble(current_item)));
      break;

    case ArgTypeEnum::String:
      arguments_list.push_back(CreateStringArgHolder(std::string(PyUnicode_AsUTF8(current_item))));
      break;

    case ArgTypeEnum::AddressToByteMap:
      pos = 0;
      address_to_byte_map = std::map<long long, s16>();
      while (PyDict_Next(current_item, &pos, &key, &value))
      {
        long long key_number = PyLong_AsLongLong(key);
        long long value_number = PyLong_AsLongLong(value);
        if (key_number < 0)
          return HandleError(class_name.c_str(), &functionMetadata, true,
                             "Key in dictionary of addresses to values was less tha 0!");
        else if (value_number < -128)
          return HandleError(class_name.c_str(), &functionMetadata, true,
                             "In address to byte map, byte value was less than -128, which can't "
                             "be represented by 1 byte!");
        else if (value_number > 255)
          return HandleError(class_name.c_str(), &functionMetadata, true,
                             "In address to byte map, byte value was greater than 255, which can't "
                             "be represented by 1 byte!");
        else
        {
          address_to_byte_map[key_number] = static_cast<s16>(value_number);
        }
      }
      arguments_list.push_back(CreateAddressToByteMapArgHolder(address_to_byte_map));
      break;

      case ArgTypeEnum::ControllerStateObject:
      controller_state_arg = {};
      memset(&controller_state_arg, 0, sizeof(Movie::ControllerState));
      controller_state_arg.is_connected = true;
      controller_state_arg.CStickX = 128;
      controller_state_arg.CStickY = 128;
      controller_state_arg.AnalogStickX = 128;
      controller_state_arg.AnalogStickY = 128;
      pos = 0;

      while (PyDict_Next(current_item, &pos, &key, &value))
      {
        const char* button_name = PyUnicode_AsUTF8(key);
        if (button_name == nullptr || button_name[0] == '\0')
        {
          return HandleError(class_name.c_str(), &functionMetadata, true,
                             "An empty string (or a value not convertible to a string) was passed "
                             "in as a button name!");
        }
        switch (ParseGCButton(button_name))
        {
        case GcButtonName::A:
          controller_state_arg.A = ExtractBoolFromObject(value);
          break;
        case GcButtonName::B:
          controller_state_arg.B = ExtractBoolFromObject(value);
          break;
        case GcButtonName::X:
          controller_state_arg.X = ExtractBoolFromObject(value);
          break;
        case GcButtonName::Y:
          controller_state_arg.Y = ExtractBoolFromObject(value);
          break;
        case GcButtonName::Z:
          controller_state_arg.Z = ExtractBoolFromObject(value);
          break;
        case GcButtonName::L:
          controller_state_arg.L = ExtractBoolFromObject(value);
          break;
        case GcButtonName::R:
          controller_state_arg.R = ExtractBoolFromObject(value);
          break;
        case GcButtonName::DPadUp:
          controller_state_arg.DPadUp = ExtractBoolFromObject(value);
          break;
        case GcButtonName::DPadDown:
          controller_state_arg.DPadDown = ExtractBoolFromObject(value);
          break;
        case GcButtonName::DPadLeft:
          controller_state_arg.DPadLeft = ExtractBoolFromObject(value);
          break;
        case GcButtonName::DPadRight:
          controller_state_arg.DPadRight = ExtractBoolFromObject(value);
          break;
        case GcButtonName::Start:
          controller_state_arg.Start = ExtractBoolFromObject(value);
          break;
        case GcButtonName::Reset:
          controller_state_arg.reset = ExtractBoolFromObject(value);
          break;
        case GcButtonName::TriggerL:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                               "Magnitude of triggerL was outside the valid bounds of 0-255");
          controller_state_arg.TriggerL = static_cast<u8>(magnitude);
          break;
        case GcButtonName::TriggerR:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                               "Magnitude of triggerR was outside the valid bounds of 0-255");
          controller_state_arg.TriggerR = static_cast<u8>(magnitude);
          break;
        case GcButtonName::AnalogStickX:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                               "Magnitude of analogStickX was outside the valid bounds of 0-255");
          controller_state_arg.AnalogStickX = static_cast<u8>(magnitude);
          break;
        case GcButtonName::AnalogStickY:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                               "Magnitude of analogStickY was outside the valid bounds of 0-255");
          controller_state_arg.AnalogStickY = static_cast<u8>(magnitude);
          break;
        case GcButtonName::CStickX:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                                "Magnitude of cStickX was outside the valid bounds of 0-255");
          controller_state_arg.CStickX = static_cast<u8>(magnitude);
          break;
        case GcButtonName::CStickY:
          magnitude = PyLong_AsLongLong(value);
          if (magnitude < 0 || magnitude > 255)
            return HandleError(class_name.c_str(), &functionMetadata, true,
                               "Magnitude of cStickY was outside the valid bounds of 0-255");
          controller_state_arg.CStickY = static_cast<u8>(magnitude);
          break;
        default:
          return HandleError(class_name.c_str(), &functionMetadata, true,
                             "Unknown button name encountered. Valid "
                             "button names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, "
                             "dPadRight, cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, "
                             "RESET, and START");
        }
      }

      arguments_list.push_back(CreateControllerStateArgHolder(controller_state_arg));
       break;
      case ArgTypeEnum::ListOfPoints:
       list_of_points = std::vector<ImVec2>();
       list_size = PyList_Size(current_item);
       for (Py_ssize_t i = 0; i < list_size; ++i)
       {
        float x = 0.0f;
        float y = 0.0f;
        PyObject* next_object_in_list = PyList_GetItem(current_item, i);
        if (PyList_Check(next_object_in_list))  // case where list contains list objects, and looks
                                                // like this: "[ [45.0, 55.0], [33.0, 22.0], ... ]"
        {
          if (PyList_Size(next_object_in_list) != 2)
            return HandleError(
                class_name.c_str(), &functionMetadata, true,
                "Expected a list of lists, where each list contains 2 float values representing X "
                "and Y points. However, sub-list did not have 2 elements!");
          x = static_cast<float>(PyFloat_AsDouble(PyList_GetItem(next_object_in_list, 0)));
          y = static_cast<float>(PyFloat_AsDouble(PyList_GetItem(next_object_in_list, 1)));
          list_of_points.push_back({x, y});
          continue;
        }
        else if (PyTuple_Check(next_object_in_list))
        {
          if (PyTuple_Size(next_object_in_list) != 2)
            return HandleError(
                class_name.c_str(), &functionMetadata, true,
                "Expected a list of tuples, where each tuple contains 2 float values representing "
                "x and y points. However, the tuple did not have 2 elements!");
          x = static_cast<float>(PyFloat_AsDouble(PyTuple_GetItem(next_object_in_list, 0)));
          y = static_cast<float>(PyFloat_AsDouble(PyTuple_GetItem(next_object_in_list, 1)));
          list_of_points.push_back({x, y});
          continue;
        }
        else
          return HandleError(
              class_name.c_str(), &functionMetadata, true,
              "Expected list of points, which would be either a list of lists or a list of tuples. "
              "However, the arguments of the list were neither of these 2 types. An example of a "
              "valid value is this: [ (45.0, 55.0), (87.3, 21.3)]");
       }
       arguments_list.push_back(CreateListOfPointsArgHolder(list_of_points));
       break;

    case ArgTypeEnum::RegistrationForButtonCallbackInputType:
       arguments_list.push_back(
           CreateRegistrationForButtonCallbackInputTypeArgHolder(*((void**)(&current_item))));
       break;
    case ArgTypeEnum::RegistrationInputType:
       arguments_list.push_back(CreateRegistrationInputTypeArgHolder((*((void**)(&current_item)))));
       break;
    case ArgTypeEnum::RegistrationWithAutoDeregistrationInputType:
       arguments_list.push_back(
           CreateRegistrationWithAutoDeregistrationInputTypeArgHolder((*((void**)(&current_item)))));
       break;
    case ArgTypeEnum::UnregistrationInputType:
       arguments_list.push_back(CreateUnregistrationInputTypeArgHolder(*((void**)(&current_item))));
       break;
    default:
     return HandleError(class_name.c_str(), &functionMetadata, true, "Argument type not supported yet!");
    }
    ++argument_index;
  }
  
  ArgHolder return_value = (*functionMetadata.function_pointer)(this_pointer, arguments_list);

  switch (return_value.argument_type)
  {
  case ArgTypeEnum::VoidType:
    Py_INCREF(Py_None);
    return Py_None;

  case ArgTypeEnum::YieldType:
    return HandleError(
        nullptr, nullptr, false,
        "EmuAPI.frameAdvance() function is not supported for Python scripts. You can register a "
        "fucntion to run once at the start of each frame with: 'funcRef = "
        "OnFrameStart.register(funcName)'. You can later stop this function from running each "
        "frame by running 'OnFrameStart.unregister(funcRef)'");

  case ArgTypeEnum::Boolean:
    if (return_value.bool_val)
      Py_RETURN_TRUE;
    else
      Py_RETURN_FALSE;
    break;

  case ArgTypeEnum::U8:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u8_val));
  case ArgTypeEnum::U16:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u16_val));
  case ArgTypeEnum::U32:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u32_val));
  case ArgTypeEnum::U64:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u64_val));

  case ArgTypeEnum::S8:
    return Py_BuildValue("L", static_cast<long long>(return_value.s8_val));
  case ArgTypeEnum::S16:
    return Py_BuildValue("L", static_cast<long long>(return_value.s16_val));
  case ArgTypeEnum::Integer:
    return Py_BuildValue("L", static_cast<long long>(return_value.int_val));
  case ArgTypeEnum::LongLong:
    return Py_BuildValue("L", return_value.long_long_val);

  case ArgTypeEnum::Float:
    return Py_BuildValue("f", return_value.float_val);
  case ArgTypeEnum::Double:
    return Py_BuildValue("d", return_value.double_val);

  case ArgTypeEnum::String:
    return Py_BuildValue("s", return_value.string_val.c_str());

  case ArgTypeEnum::AddressToUnsignedByteMap:
    return_dictionary_object = PyDict_New();
    for (auto& it : return_value.address_to_unsigned_byte_map)
    {
      PyDict_SetItem(return_dictionary_object, PyLong_FromLongLong(it.first),
                     PyLong_FromLongLong(static_cast<long long>(it.second)));
    }
    return return_dictionary_object;

  case ArgTypeEnum::AddressToSignedByteMap:
    return_dictionary_object = PyDict_New();
    for (auto& it : return_value.address_to_signed_byte_map)
    {
      PyDict_SetItem(return_dictionary_object, PyLong_FromLongLong(it.first), PyLong_FromLongLong(static_cast<long long>(it.second)));
    }
    return return_dictionary_object;

  case ArgTypeEnum::ControllerStateObject:
    controller_state_arg = return_value.controller_state_val;
    return_dictionary_object = PyDict_New();
    for (GcButtonName button_code : GetListOfAllButtons())
    {
      PyObject* button_name_object = PyUnicode_FromString(ConvertButtonEnumToString(button_code));
      switch (button_code)
      {
      case GcButtonName::A:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.A)));
        break;
      case GcButtonName::B:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.B)));
        break;
      case GcButtonName::X:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.X)));
        break;
      case GcButtonName::Y:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.Y)));
        break;
      case GcButtonName::Z:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.Z)));
        break;
      case GcButtonName::L:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.L)));
        break;
      case GcButtonName::R:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.R)));
        break;
      case GcButtonName::DPadUp:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.DPadUp)));
        break;
      case GcButtonName::DPadDown:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.DPadDown)));
        break;
      case GcButtonName::DPadLeft:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.DPadLeft)));
        break;
      case GcButtonName::DPadRight:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.DPadRight)));
        break;
      case GcButtonName::Start:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.Start)));
        break;
      case GcButtonName::Reset:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyBool_FromLong(static_cast<long long>(controller_state_arg.reset)));
        break;
      case GcButtonName::TriggerL:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.TriggerL)));
        break;
      case GcButtonName::TriggerR:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.TriggerR)));
        break;
      case GcButtonName::AnalogStickX:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.AnalogStickX)));
        break;
      case GcButtonName::AnalogStickY:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.AnalogStickY)));
        break;
      case GcButtonName::CStickX:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.CStickX)));
        break;
      case GcButtonName::CStickY:
        PyDict_SetItem(return_dictionary_object, button_name_object,
                       PyLong_FromLong(static_cast<long>(controller_state_arg.CStickY)));
        break;
      default:
        return HandleError(class_name.c_str(), &functionMetadata, true,
                           "Unknown button name encountered when returning GC controller values "
                           "from function. Did you update the buttons in the GCButtons.h file and "
                           "then forget to update the list of all buttons?");
      }
    }
    return return_dictionary_object;

    case ArgTypeEnum::RegistrationReturnType:
    if (PyErr_Occurred())
    {
      break;
    }
      return (*((PyObject**)&(return_value.void_pointer_val)));
    case ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType:
    case ArgTypeEnum::UnregistrationReturnType:
      Py_INCREF(Py_None);
      return Py_None;
    case ArgTypeEnum::ShutdownType:
      this_pointer->ShutdownScript();
      Py_INCREF(Py_None);
      return Py_None;
  case ArgTypeEnum::ErrorStringType:
    return HandleError(class_name.c_str(), &functionMetadata, true, return_value.error_string_val);

  default:
   return HandleError(class_name.c_str(), &functionMetadata, true, "Return Type not supported yet!");
  }
  return nullptr;
}

void PythonScriptContext::ImportModule(const std::string& api_name, const std::string& api_version)
{
  PyObject* current_module = PyImport_ImportModule(api_name.c_str());

  if (current_module == nullptr)
   return;

  SetModuleVersion(api_name.c_str(), api_version);
  RunImportCommand(api_name.c_str());
}

void PythonScriptContext::StartScript()
{
  PyEval_RestoreThread(main_python_thread);
  FILE* fp = fopen(script_filename.c_str(), "rb");
  PyRun_AnyFile(fp, nullptr);
  fclose(fp);
  this->finished_with_global_code = true;
  this->RunEndOfIteraionTasks();
  PyEval_ReleaseThread(main_python_thread);
}

void PythonScriptContext::RunGlobalScopeCode()
{
  return; // python scripts can't run on the global scope - except for when a script is started for the very first time, which is handld in the StartScript function above
}

void PythonScriptContext::RunCallbacksForVector(std::vector<PyObject*>& callback_list)
{
  if (ShouldCallEndScriptFunction())
  {
   ShutdownScript();
   return;
  }

  if (!this->finished_with_global_code)
   return;


  PyEval_RestoreThread(main_python_thread);

  for (int i = 0; i < callback_list.size(); ++i)
  {
   PyObject* func = callback_list[i];
   Py_INCREF(func);
   PyObject* return_value = PyObject_CallFunction(func, nullptr);
   Py_DECREF(func);
   if (return_value == nullptr)
      break;
  }

  this->RunEndOfIteraionTasks();
  PyEval_ReleaseThread(main_python_thread);
}
void PythonScriptContext::RunCallbacksForMap(std::unordered_map<size_t, std::vector<PyObject*>>& map_of_callbacks, size_t current_address)
{
  if (ShouldCallEndScriptFunction())
  {
   ShutdownScript();
   return;
  }

  if (!this->finished_with_global_code)
   return;

  PyEval_RestoreThread(main_python_thread);
  if (map_of_callbacks.count(current_address) == 0)
   return;

  std::vector<PyObject*>* callbacks_for_address = &map_of_callbacks[current_address];

  if (callbacks_for_address == nullptr || callbacks_for_address->empty())
   return;

  for (int i = 0; i < callbacks_for_address->size(); ++i)
  {
   PyObject* func = (*callbacks_for_address)[i];
   Py_IncRef(func);
   PyObject* return_value = PyObject_CallFunction(func, nullptr);
   Py_DECREF(func);
   if (return_value == nullptr)
      break;
  }

  this->RunEndOfIteraionTasks();
  PyEval_ReleaseThread(main_python_thread);
}

void* PythonScriptContext::RegisterForVectorHelper(std::vector<PyObject*>& callback_list,
                                                  void* callback)
{
  PyObject* func = *((PyObject**)&callback);
  if (func == nullptr || !PyCallable_Check(func))
    HandleError(nullptr, nullptr, false,
               "Attempted to add a non-callable function to the list of callbacks with a register "
               "function call!");
  else
  {
   Py_INCREF(func);
   callback_list.push_back(func);
  }
  return callback;
 }

void PythonScriptContext::RegisterForVectorWithAutoDeregistrationHelper(
    std::vector<PyObject*>& callback_list, void* callback,
     std::atomic<size_t>& number_of_callbacks_to_auto_deregister)
{
  PyObject* func = *((PyObject**)&callback);
  if (func == nullptr || !PyCallable_Check(func))
    HandleError(nullptr, nullptr, false,
                "Attempted to add a non-callable function to the list of callbacks with a "
                "registerWithAutoDeregistration function call!");
  else
  {
    Py_INCREF(func);
    callback_list.push_back(func);
    ++number_of_callbacks_to_auto_deregister;
  }
}

bool PythonScriptContext::UnregisterForVectorHelper(std::vector<PyObject*>& callback_list,
                                                    void* callback)
{
  PyObject* func = *((PyObject**)&callback);
  if (func == nullptr || !PyCallable_Check(func))
  {
    HandleError(
        nullptr, nullptr, false,
        "Attempted to pass a non-callable function as an argument to the unregister function!");
    return false;
  }
  if (std::find(callback_list.begin(), callback_list.end(), func) == callback_list.end())
  {
    HandleError(nullptr, nullptr, false,
                "Attempted to unregister a function which wasn't registered!");
    return false;
  }
  Py_DECREF(func);
  callback_list.erase(std::find(callback_list.begin(), callback_list.end(), func));
  return true;
}

void* PythonScriptContext::RegisterForMapHelper(size_t address,
                           std::unordered_map<size_t, std::vector<PyObject*>>& map_of_callbacks, void* callbacks)
{
  PyObject* func = *((PyObject**)&callbacks);
  if (func == nullptr || !PyCallable_Check(func))
  {
    HandleError(nullptr, nullptr, false, "Attempted to register function which was not callable!");
    return nullptr;
  }
  else
  {
    if (map_of_callbacks.count(address) == 0)
      map_of_callbacks[address] = std::vector<PyObject*>();
    Py_INCREF(func);
    map_of_callbacks[address].push_back(func);
    return callbacks;
  }
}

void PythonScriptContext::RegisterForMapWithAutoDeregistrationHelper(
    size_t address, std::unordered_map<size_t, std::vector<PyObject*>>& map_of_callbacks, void* callbacks, std::atomic<size_t>& number_of_auto_deregistration_callbacks)
{
  PyObject* func = *((PyObject**)&callbacks);
  if (func == nullptr || !PyCallable_Check(func))
  {
    HandleError(nullptr, nullptr, false,
                "Attempted to registerWithAutoDeregistration a function which was not callable!");
  }
  else
  {
    if (map_of_callbacks.count(address) == 0)
      map_of_callbacks[address] = std::vector<PyObject*>();
    Py_INCREF(func);
    map_of_callbacks[address].push_back(func);
    ++number_of_auto_deregistration_callbacks;
  }
}

bool PythonScriptContext::UnregisterForMapHelper(size_t address,
                            std::unordered_map<size_t, std::vector<PyObject*>>& map_of_callbacks,
                            void* callbacks)
{
  PyObject* func = *((PyObject**)&callbacks);
  if (func == nullptr || !PyCallable_Check(func))
  {
    HandleError(nullptr, nullptr, false, "Attempted to unregister an invalid function!");
    return false;
  }
  else
  {
    if (map_of_callbacks.count(address) == 0)
    {
      HandleError(nullptr, nullptr, false,
                  "Attempted to unregister a function which was not registered!");
      return false;
    }
    std::vector<PyObject*>* reference_to_vector = &map_of_callbacks[address];
    if (std::find(reference_to_vector->begin(), reference_to_vector->end(), func) ==
        reference_to_vector->end())
    {
      HandleError(nullptr, nullptr, false,
                  "Attmpted to unregister a function which was not registered!");
      return false;
    }
    Py_DECREF(func);
    reference_to_vector->erase(
        std::find(reference_to_vector->begin(), reference_to_vector->end(), func));
    return true;

  }
}

void PythonScriptContext::RunOnFrameStartCallbacks()
{
  if (this->frame_callbacks.size() == 0)
    return;
  this->RunCallbacksForVector(this->frame_callbacks);
}

void PythonScriptContext::RunOnGCControllerPolledCallbacks()
{
  this->RunCallbacksForVector(this->gc_controller_input_polled_callbacks);
}

void PythonScriptContext::RunOnInstructionReachedCallbacks(size_t current_address)
{
  this->RunCallbacksForMap(this->map_of_instruction_address_to_python_callbacks, current_address);
}

void PythonScriptContext::RunOnMemoryAddressReadFromCallbacks(size_t current_memory_address)
{
  this->RunCallbacksForMap(this->map_of_memory_address_read_from_to_python_callbacks,
                          current_memory_address);
}

void PythonScriptContext::RunOnMemoryAddressWrittenToCallbacks(size_t current_memory_address)
{
  this->RunCallbacksForMap(this->map_of_memory_address_written_to_to_python_callbacks,
                          current_memory_address);
}

void PythonScriptContext::RunOnWiiInputPolledCallbacks()
{
  this->RunCallbacksForVector(this->wii_controller_input_polled_callbacks);
}

void* PythonScriptContext::RegisterOnFrameStartCallbacks(void* callbacks)
{
  return this->RegisterForVectorHelper(this->frame_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnFrameStartWithAutoDeregistrationCallbacks(void* callbacks)
{
  this->RegisterForVectorWithAutoDeregistrationHelper(
      this->frame_callbacks, callbacks, this->number_of_frame_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnFrameStartCallbacks(void* callbacks)
{
  return this->UnregisterForVectorHelper(this->frame_callbacks, callbacks);
}

void* PythonScriptContext::RegisterOnGCCControllerPolledCallbacks(void* callbacks)
{
  return this->RegisterForVectorHelper(this->gc_controller_input_polled_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks(
    void* callbacks)
{
  this->RegisterForVectorWithAutoDeregistrationHelper(
      this->gc_controller_input_polled_callbacks, callbacks,
      this->number_of_gc_controller_input_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnGCControllerPolledCallbacks(void* callbacks)
{
  return this->UnregisterForVectorHelper(this->gc_controller_input_polled_callbacks, callbacks);
}

void* PythonScriptContext::RegisterOnInstructionReachedCallbacks(size_t address, void* callbacks)
{
  return this->RegisterForMapHelper(
      address, this->map_of_instruction_address_to_python_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(
    size_t address, void* callbacks)
{
  this->RegisterForMapWithAutoDeregistrationHelper(
      address, this->map_of_instruction_address_to_python_callbacks, callbacks,
      this->number_of_instruction_address_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnInstructionReachedCallbacks(size_t address, void* callbacks)
{
  return this->UnregisterForMapHelper(address, this->map_of_instruction_address_to_python_callbacks,
                               callbacks);
}

void* PythonScriptContext::RegisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                                    void* callbacks)
{
  return this->RegisterForMapHelper(
      memory_address, this->map_of_memory_address_read_from_to_python_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(
    size_t memory_address, void* callbacks)
{
  this->RegisterForMapWithAutoDeregistrationHelper(
      memory_address, this->map_of_memory_address_read_from_to_python_callbacks, callbacks,
      this->number_of_memory_address_read_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                                     void* callbacks)
{
  return this->UnregisterForMapHelper(
      memory_address, this->map_of_memory_address_read_from_to_python_callbacks, callbacks);
}

void* PythonScriptContext::RegisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                                     void* callbacks)
{
  return this->RegisterForMapHelper(
      memory_address, this->map_of_memory_address_written_to_to_python_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(
    size_t memory_address, void* callbacks)
{
  this->RegisterForMapWithAutoDeregistrationHelper(
      memory_address, this->map_of_memory_address_written_to_to_python_callbacks, callbacks,
      this->number_of_memory_address_write_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                                      void* callbacks)
{
  return this->UnregisterForMapHelper(
      memory_address, this->map_of_memory_address_written_to_to_python_callbacks, callbacks);
}

void* PythonScriptContext::RegisterOnWiiInputPolledCallbacks(void* callbacks)
{
  return this->RegisterForVectorHelper(this->wii_controller_input_polled_callbacks, callbacks);
}

void PythonScriptContext::RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks(void* callbacks)
{
  this->RegisterForVectorWithAutoDeregistrationHelper(
      this->wii_controller_input_polled_callbacks, callbacks,
      number_of_wii_input_callbacks_to_auto_deregister);
}

bool PythonScriptContext::UnregisterOnWiiInputPolledCallbacks(void* callbacks)
{
  return this->UnregisterForVectorHelper(this->wii_controller_input_polled_callbacks, callbacks);
}

void PythonScriptContext::RegisterButtonCallback(long long button_id, void* callbacks)
{
  PyObject* func = *((PyObject**)&callbacks);
  if (func == nullptr || !PyCallable_Check(func))
    HandleError(nullptr, nullptr, false,
                "Attempted to register button callback which was not a callable function!");
  else
  {
    map_of_button_id_to_callback[button_id] = func;
  }
}

bool PythonScriptContext::IsButtonRegistered(long long button_id)
{
  return map_of_button_id_to_callback.count(button_id) != 0;
}

void PythonScriptContext::GetButtonCallbackAndAddToQueue(long long button_id)
{
  if (map_of_button_id_to_callback.count(button_id) == 0)
    HandleError(nullptr, nullptr, false,
                "Attempted to press button with an undefined function callback!");
  else
  {
    PyObject* callback_func = map_of_button_id_to_callback[button_id];
    button_callbacks_to_run.push(callback_func);
  }
}

void PythonScriptContext::RunButtonCallbacksInQueue()
{
  if (button_callbacks_to_run.IsEmpty())
    return;
  std::vector<PyObject*> list_of_functions;

  while (!button_callbacks_to_run.IsEmpty())
  {
    list_of_functions.push_back(button_callbacks_to_run.pop());
  }
  this->RunCallbacksForVector(list_of_functions);
}

PyObject* PythonScriptContext::HandleError(const char* class_name, const FunctionMetadata* function_metadata, bool include_example, const std::string& base_error_msg)
{
  std::string error_msg = "";
  if (class_name == nullptr)
  {
   if (function_metadata == nullptr)
      error_msg = fmt::format("Error: {}", base_error_msg);
   else
   {
      if (include_example)
        error_msg = fmt::format("Error: In {}(), {} The method should be called like this: {}",
                                function_metadata->function_name, base_error_msg,
                                function_metadata->example_function_call);
      else
        error_msg =
            fmt::format("Error: In {}(), {}", function_metadata->function_name, base_error_msg);
   }
  }
  else
  {
   if (function_metadata == nullptr)
      error_msg =
          fmt::format("Error: In function call in {} module, {}", class_name, base_error_msg);
   else
   {
      if (include_example)
        error_msg =
            fmt::format("Error: In {}.{}, {} The method should be called like this: {}.{}",
                        class_name, function_metadata->function_name, base_error_msg, class_name,
                        function_metadata->example_function_call);
      else
        error_msg = fmt::format("Error: In {}.{}, {}", class_name,
                                function_metadata->function_name, base_error_msg);
   }
  }

  PyErr_SetString(PyExc_RuntimeError, error_msg.c_str());
  return nullptr;
}

void PythonScriptContext::RunEndOfIteraionTasks()
{
  if (PyErr_Occurred())
  {
   PyErr_PrintEx(1);
  }

  PyObject* pModule = PyImport_ImportModule(redirect_output_module_name);
  PyObject* catcher = PyObject_GetAttrString(pModule, "catchOutErr");  // get our catchOutErr created above
  PyObject* output = PyObject_GetAttrString(catcher, "value");

  const char* output_msg = PyUnicode_AsUTF8(output);
  if (output_msg != nullptr && !std::string(output_msg).empty())
   (*GetPrintCallback())(output_msg);

  PyObject* clear_method = PyObject_GetAttrString(catcher, "clear");
  PyObject_CallFunction(clear_method, nullptr);

  if (ShouldCallEndScriptFunction())
   ShutdownScript();
}

}  // namespace Scripting::Python
