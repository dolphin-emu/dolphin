#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/GraphicsModuleImporter.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::GraphicsModuleImporter
{
static std::string graphics_class_name = GraphicsAPI::class_name;

static const char* draw_line_function_name = "drawLine";
static const char* draw_empty_rectangle_function_name = "drawEmptyRectangle";
static const char* draw_filled_rectangle_function_name = "drawFilledRectangle";
static const char* draw_empty_triangle_function_name = "drawEmptyTriangle";
static const char* draw_filled_triangle_function_name = "drawFilledTriangle";
static const char* draw_empty_circle_function_name = "drawEmptyCircle";
static const char* draw_filled_circle_function_name = "drawFilledCircle";
static const char* draw_empty_polygon_function_name = "drawEmptyPolygon";
static const char* draw_filled_polygon_function_name = "drawFilledPolygon";
static const char* draw_text_function_name = "drawText";
static const char* add_checkbox_function_name = "addCheckbox";
static const char* get_checkbox_value_function_name = "getCheckboxValue";
static const char* set_checkbox_value_function_name = "setCheckboxValue";
static const char* add_radio_button_group_function_name = "addRadioButtonGroup";
static const char* add_radio_button_function_name = "addRadioButton";
static const char* get_radio_button_group_value_function_name = "getRadioButtonGroupValue";
static const char* set_radio_button_group_value_function_name = "setRadioButtonGroupValue";
static const char* add_text_box_function_name = "addTextBox";
static const char* get_text_box_value_function_name = "getTextBoxValue";
static const char* set_text_box_value_function_name = "setTextBoxValue";
static const char* add_button_function_name = "addButton";
static const char* press_button_function_name = "pressButton";
static const char* new_line_function_name = "newLine";
static const char* begin_window_function_name = "beginWindow";
static const char* end_window_function_name = "endWindow";

static PyObject* python_draw_line(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name, draw_line_function_name);
}

static PyObject* python_draw_empty_rectangle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_empty_rectangle_function_name);
}

static PyObject* python_draw_filled_rectangle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_filled_rectangle_function_name);
}

static PyObject* python_draw_empty_triangle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_empty_triangle_function_name);
}

static PyObject* python_draw_filled_triangle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_filled_triangle_function_name);
}

static PyObject* python_draw_empty_circle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_empty_circle_function_name);
}

static PyObject* python_draw_filled_circle(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_filled_circle_function_name);
}

static PyObject* python_draw_empty_polygon(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_empty_polygon_function_name);
}

static PyObject* python_draw_filled_polygon(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          draw_filled_polygon_function_name);
}

static PyObject* python_draw_text(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name, draw_text_function_name);
}

static PyObject* python_add_checkbox(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          add_checkbox_function_name);
}

static PyObject* python_get_checkbox_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          get_checkbox_value_function_name);
}

static PyObject* python_set_checkbox_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          set_checkbox_value_function_name);
}

static PyObject* python_add_radio_button_group(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          add_radio_button_group_function_name);
}

static PyObject* python_add_radio_button(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          add_radio_button_function_name);
}

static PyObject* python_get_radio_button_group_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          get_radio_button_group_value_function_name);
}

static PyObject* python_set_radio_button_group_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          set_radio_button_group_value_function_name);
}

static PyObject* python_add_text_box(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          add_text_box_function_name);
}

static PyObject* python_get_text_box_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          get_text_box_value_function_name);
}

static PyObject* python_set_text_box_value(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          set_text_box_value_function_name);
}

static PyObject* python_add_button(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          add_button_function_name);
}

static PyObject* python_press_button(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          press_button_function_name);
}

static PyObject* python_new_line(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name, new_line_function_name);
}

static PyObject* python_begin_window(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          begin_window_function_name);
}

static PyObject* python_end_window(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, graphics_class_name,
                                          end_window_function_name);
}

static PyMethodDef graphics_api_methods[] = {
    {draw_line_function_name, python_draw_line, METH_VARARGS, nullptr},
    {draw_empty_rectangle_function_name, python_draw_empty_rectangle, METH_VARARGS, nullptr},
    {draw_filled_rectangle_function_name, python_draw_filled_rectangle, METH_VARARGS, nullptr},
    {draw_empty_triangle_function_name, python_draw_empty_triangle, METH_VARARGS, nullptr},
    {draw_filled_triangle_function_name, python_draw_filled_triangle, METH_VARARGS, nullptr},
    {draw_empty_circle_function_name, python_draw_empty_circle, METH_VARARGS, nullptr},
    {draw_filled_circle_function_name, python_draw_filled_circle, METH_VARARGS, nullptr},
    {draw_empty_polygon_function_name, python_draw_empty_polygon, METH_VARARGS, nullptr},
    {draw_filled_polygon_function_name, python_draw_filled_polygon, METH_VARARGS, nullptr},
    {draw_text_function_name, python_draw_text, METH_VARARGS, nullptr},
    {add_checkbox_function_name, python_add_checkbox, METH_VARARGS, nullptr},
    {get_checkbox_value_function_name, python_get_checkbox_value, METH_VARARGS, nullptr},
    {set_checkbox_value_function_name, python_set_checkbox_value, METH_VARARGS, nullptr},
    {add_radio_button_group_function_name, python_add_radio_button_group, METH_VARARGS, nullptr},
    {add_radio_button_function_name, python_add_radio_button, METH_VARARGS, nullptr},
    {get_radio_button_group_value_function_name, python_get_radio_button_group_value, METH_VARARGS,
     nullptr},
    {set_radio_button_group_value_function_name, python_set_radio_button_group_value, METH_VARARGS,
     nullptr},
    {add_text_box_function_name, python_add_text_box, METH_VARARGS, nullptr},
    {get_text_box_value_function_name, python_get_text_box_value, METH_VARARGS, nullptr},
    {set_text_box_value_function_name, python_set_text_box_value, METH_VARARGS, nullptr},
    {add_button_function_name, python_add_button, METH_VARARGS, nullptr},
    {press_button_function_name, python_press_button, METH_VARARGS, nullptr},
    {new_line_function_name, python_new_line, METH_VARARGS, nullptr},
    {begin_window_function_name, python_begin_window, METH_VARARGS, nullptr},
    {end_window_function_name, python_end_window, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef GraphicsAPImodule = {PyModuleDef_HEAD_INIT, graphics_class_name.c_str(),
                                               "GraphicsAPI Module", sizeof(std::string),
                                               graphics_api_methods};

PyMODINIT_FUNC PyInit_GraphicsAPI()
{
  return PyModule_Create(&GraphicsAPImodule);
}

}  // namespace Scripting::Python::GraphicsModuleImporter
