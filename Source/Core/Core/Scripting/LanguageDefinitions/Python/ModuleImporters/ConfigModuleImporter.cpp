#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ConfigModuleImporter.h"

#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"

#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::ConfigModuleImporter
{
static std::string config_class_name = ConfigAPI::class_name;

static const char* get_layer_names_most_global_first_function_name = "getLayerNames_mostGlobalFirst";
static const char* does_layer_exist_function_name = "doesLayerExist";
static const char* get_list_of_systems_function_name = "getListOfSystems";
static const char* get_config_enum_types_function_name = "getConfigEnumTypes";
static const char* get_list_of_valid_values_for_enum_type_function_name = "getListOfValidValuesForEnumType";

static const char* get_boolean_config_setting_for_layer_function_name = "getBooleanConfigSettingForLayer";
static const char* get_signed_int_config_setting_for_layer_function_name = "getSignedIntConfigSettingForLayer";
static const char* get_unsigned_int_config_setting_for_layer_function_name = "getUnsignedIntConfigSettingForLayer";
static const char* get_float_config_setting_for_layer_function_name = "getFloatConfigSettingForLayer";
static const char* get_string_config_setting_for_layer_function_name = "getStringConfigSettingForLayer";
static const char* get_enum_config_setting_for_layer_function_name = "getEnumConfigSettingForLayer";

static const char* set_boolean_config_setting_for_layer_function_name = "setBooleanConfigSettingForLayer";
static const char* set_signed_int_config_setting_for_layer_function_name = "setSignedIntConfigSettingForLayer";
static const char* set_unsigned_int_config_setting_for_layer_function_name = "setUnsignedIntConfigSettingForLayer";
static const char* set_float_config_setting_for_layer_function_name = "setFloatConfigSettingForLayer";
static const char* set_string_config_setting_for_layer_function_name = "setStringConfigSettingForLayer";
static const char* set_enum_config_setting_for_layer_function_name = "setEnumConfigSettingForLayer";

static const char* get_boolean_config_setting_function_name = "getBooleanConfigSetting";
static const char* get_signed_int_config_setting_function_name = "getSignedIntConfigSetting";
static const char* get_unsigned_int_config_setting_function_name = "getUnsignedIntConfigSetting";
static const char* get_float_config_setting_function_name = "getFloatConfigSetting";
static const char* get_string_config_setting_function_name = "getStringConfigSetting";
static const char* get_enum_config_setting_function_name = "getEnumConfigSetting";

static const char* save_settings_function_name = "saveSettings";


static PyObject* python_get_layer_names_most_global_first(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_layer_names_most_global_first_function_name);
}

static PyObject* python_does_layer_exist(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, does_layer_exist_function_name);
}

static PyObject* python_get_list_of_systems(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_list_of_systems_function_name);
}

static PyObject* python_get_config_enum_types(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_config_enum_types_function_name);
}

static PyObject* python_get_list_of_valid_values_for_enum_type(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_list_of_valid_values_for_enum_type_function_name);
}


static PyObject* python_get_boolean_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_boolean_config_setting_for_layer_function_name);
}

static PyObject* python_get_signed_int_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_signed_int_config_setting_for_layer_function_name);
}

static PyObject* python_get_unsigned_int_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_unsigned_int_config_setting_for_layer_function_name);
}

static PyObject* python_get_float_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_float_config_setting_for_layer_function_name);
}

static PyObject* python_get_string_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_string_config_setting_for_layer_function_name);
}

static PyObject* python_get_enum_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_enum_config_setting_for_layer_function_name);
}


static PyObject* python_set_boolean_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_boolean_config_setting_for_layer_function_name);
}

static PyObject* python_set_signed_int_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_signed_int_config_setting_for_layer_function_name);
}

static PyObject* python_set_unsigned_int_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_unsigned_int_config_setting_for_layer_function_name);
}

static PyObject* python_set_float_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_float_config_setting_for_layer_function_name);
}

static PyObject* python_set_string_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_string_config_setting_for_layer_function_name);
}

static PyObject* python_set_enum_config_setting_for_layer(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, set_enum_config_setting_for_layer_function_name);
}


static PyObject* python_get_boolean_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_boolean_config_setting_function_name);
}

static PyObject* python_get_signed_int_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_signed_int_config_setting_function_name);
}

static PyObject* python_get_unsigned_int_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_unsigned_int_config_setting_function_name);
}

static PyObject* python_get_float_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_float_config_setting_function_name);
}

static PyObject* python_get_string_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_string_config_setting_function_name);
}

static PyObject* python_get_enum_config_setting(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, get_enum_config_setting_function_name);
}


static PyObject* python_save_settings(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, config_class_name, save_settings_function_name);
}


static PyMethodDef config_api_methods[] = {
    {get_layer_names_most_global_first_function_name, python_get_layer_names_most_global_first, METH_VARARGS, nullptr},
    {does_layer_exist_function_name, python_does_layer_exist, METH_VARARGS, nullptr},
    {get_list_of_systems_function_name, python_get_list_of_systems, METH_VARARGS, nullptr},
    {get_config_enum_types_function_name, python_get_config_enum_types, METH_VARARGS, nullptr},
    {get_list_of_valid_values_for_enum_type_function_name, python_get_list_of_valid_values_for_enum_type, METH_VARARGS, nullptr},
    {get_boolean_config_setting_for_layer_function_name, python_get_boolean_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_signed_int_config_setting_for_layer_function_name, python_get_signed_int_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_unsigned_int_config_setting_for_layer_function_name, python_get_unsigned_int_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_float_config_setting_for_layer_function_name, python_get_float_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_string_config_setting_for_layer_function_name, python_get_string_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_enum_config_setting_for_layer_function_name, python_get_enum_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_boolean_config_setting_for_layer_function_name, python_set_boolean_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_signed_int_config_setting_for_layer_function_name, python_set_signed_int_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_unsigned_int_config_setting_for_layer_function_name, python_set_unsigned_int_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_float_config_setting_for_layer_function_name, python_set_float_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_string_config_setting_for_layer_function_name, python_set_string_config_setting_for_layer, METH_VARARGS, nullptr},
    {set_enum_config_setting_for_layer_function_name, python_set_enum_config_setting_for_layer, METH_VARARGS, nullptr},
    {get_boolean_config_setting_function_name, python_get_boolean_config_setting, METH_VARARGS, nullptr},
    {get_signed_int_config_setting_function_name, python_get_signed_int_config_setting, METH_VARARGS, nullptr},
    {get_unsigned_int_config_setting_function_name, python_get_unsigned_int_config_setting, METH_VARARGS, nullptr},
    {get_float_config_setting_function_name, python_get_float_config_setting, METH_VARARGS, nullptr},
    {get_string_config_setting_function_name, python_get_string_config_setting, METH_VARARGS, nullptr},
    {get_enum_config_setting_function_name, python_get_enum_config_setting, METH_VARARGS, nullptr},
    {save_settings_function_name, python_save_settings, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef ConfigAPImodule = {
    PyModuleDef_HEAD_INIT, config_class_name.c_str(), /* name of module */
    "ConfigAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    config_api_methods};

PyMODINIT_FUNC PyInit_ConfigAPI()
{
  return PyModule_Create(&ConfigAPImodule);
}
}  // namespace Scripting::Python::ConfigModuleImporter
