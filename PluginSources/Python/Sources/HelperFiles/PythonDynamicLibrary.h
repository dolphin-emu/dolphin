#ifndef PYTHON_DYNAMIC_LIBRARY
#define PYTHON_DYNAMIC_LIBRARY

#include "DynamicLibrary.h"

extern DynamicLibrary* python_lib_ptr;

bool InitPythonLib();
void DeletePythonLib();

extern const char* CAPSULE_GET_POINTER_NAME;
extern const char* IMPORT_MODULE_NAME;
extern const char* MODULE_GET_STATE_NAME;
extern const char* INC_REF_NAME;
extern const char* DEC_REF_NAME;
extern const char* PY_NONE_STRUCT_NAME;
extern const char* PY_TRUE_STRUCT_NAME;
extern const char* PY_FALSE_STRUCT_NAME;
extern const char* PYTHON_BUILD_VALUE_NAME;
extern const char* PYTHON_RUN_STRING_NAME;
extern const char* PYTHON_RUN_FILE_NAME;
extern const char* PYTHON_SET_ERR_NAME;
extern const char* PYTHON_ERROR_OCCURED_NAME;
extern const char* PY_ERR_PRINT_EX_NAME;
extern const char* PY_OBJECT_GET_ATTR_STRING_NAME;
extern const char* PY_UNICODE_AS_UTF8_NAME;
extern const char* PY_OBJECT_CALL_FUNCTION_NAME;
extern const char* PY_INITIALIZE_NAME;
extern const char* PY_THREAD_STATE_GET_NAME;
extern const char* PY_EVAL_RESTORE_THREAD_NAME;
extern const char* PY_EVAL_RELEASE_THREAD_NAME;
extern const char* PY_NEW_INTERPRETER_NAME;
extern const char* PY_MODULE_CREATE_NAME;
extern const char* APPEND_INIT_TAB_SYMBOL_NAME;
extern const char* PY_UNICODE_AS_STRING_NAME;
extern const char* PY_MODULE_GET_DICT_NAME;
extern const char* PY_CAPSULE_NEW_NAME;
extern const char* PY_CFUNCTION_NEW_EX;
extern const char* PY_DICT_SET_STRING_NAME;
extern const char* PY_CALLABLE_CHECK_NAME;
extern const char* PY_IS_TRUE_NAME;
extern const char* PY_TUPLE_GET_SIZE_NAME;
extern const char* PY_TUPLE_GET_ITEM_NAME;
extern const char* PY_TUPLE_CHECK_NAME;
extern const char* PY_LONG_AS_UNSIGNED_LONG_LONG_NAME;
extern const char* PY_LONG_AS_LONG_LONG_NAME;
extern const char* PY_LONG_FROM_LONG_LONG_NAME;
extern const char* PY_LONG_FROM_UNSIGNED_LONG_LONG_NAME;
extern const char* PY_FLOAT_AS_DOUBLE_NAME;
extern const char* PY_DICT_NEW_NAME;
extern const char* PY_DICT_SET_ITEM_NAME;
extern const char* PY_DICT_NEXT_NAME;
extern const char* PY_LIST_SIZE_NAME;
extern const char* PY_LIST_GET_ITEM_NAME;
extern const char* PY_LIST_CHECK_NAME;
extern const char* PY_UNICODE_FROM_STRING_NAME;
extern const char* PY_BOOL_FROM_LONG_NAME;

#endif
