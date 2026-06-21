// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "guimodule.h"

#include "Common/Logging/Log.h"
#include "Core/API/Gui.h"
#include "Scripting/Python/PyScriptingBackend.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

struct GuiModuleState
{
  API::Gui* gui;
};

static void add_osd_message(PyObject* self, const char* message, u32 duration_ms, u32 color_argb)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->AddOSDMessage(std::string(message), duration_ms, color_argb);
}

static void clear_osd_messages(PyObject* self)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->ClearOSDMessages();
}

static PyObject* get_display_size(PyObject* self, PyObject* args)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  auto size = state->gui->GetDisplaySize();
  return Py_BuildValue("(ff)", size.x, size.y);
}

static void draw_line(PyObject* self, float ax, float ay, float bx, float by, u32 color, float thickness = 1.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawLine({ax, ay}, {bx, by}, color, thickness);
}

static void draw_rect(PyObject* self, float ax, float ay, float bx, float by, u32 color,
               float rounding = 0.0f, float thickness = 1.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawRect({ax, ay}, {bx, by}, color, rounding, thickness);
}

static void draw_rect_filled(PyObject* self, float ax, float ay, float bx, float by, u32 color,
                      float rounding = 0.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawRectFilled({ax, ay}, {bx, by}, color, rounding);
}

static void draw_quad(PyObject* self, float ax, float ay, float bx, float by, float cx, float cy, float dx,
               float dy, u32 color, float thickness = 1.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawQuad({ax, ay}, {bx, by}, {cx, cy}, {dx, dy}, color, thickness);
}

static void draw_quad_filled(PyObject* self, float ax, float ay, float bx, float by, float cx, float cy,
                      float dx, float dy, u32 color)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawQuadFilled({ax, ay}, {bx, by}, {cx, cy}, {dx, dy}, color);
}

static void draw_triangle(PyObject* self, float ax, float ay, float bx, float by, float cx, float cy,
                   u32 color, float thickness = 1.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawTriangle({ax, ay}, {bx, by}, {cx, cy}, color, thickness);
}

static void draw_triangle_filled(PyObject* self, float ax, float ay, float bx, float by, float cx,
                          float cy, u32 color)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawTriangleFilled({ax, ay}, {bx, by}, {cx, cy}, color);
}

static void draw_circle(PyObject* self, float centerX, float centerY, float radius, u32 color,
                 int num_segments = 12, float thickness = 1.0f)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawCircle({centerX, centerY}, radius, color, num_segments, thickness);
}

static void draw_circle_filled(PyObject* self, float centerX, float centerY, float radius, u32 color,
                        int num_segments = 12)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawCircleFilled({centerX, centerY}, radius, color, num_segments);
}

static void draw_text(PyObject* self, float posX, float posY, u32 color, const char* text)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawText({posX, posY}, color, std::string(text));
}

static PyObject* draw_polyline(PyObject* self, PyObject* args)
{
  PyObject* points_list_obj;
  u32 color;
  bool closed;
  float thickness;
  if (!PyArg_ParseTuple(args, "O!Ipf", &PyList_Type, &points_list_obj, &color, &closed,
                        &thickness))
    return nullptr;
  int num_points = PyList_Size(points_list_obj);
  if (num_points < 0)
    return nullptr;
  std::vector<Vec2f> points_collecting;
  for (int i = 0; i < num_points; ++i)
  {
    PyObject* item = PyList_GetItem(points_list_obj, i);
    float x, y;
    if (!PyArg_ParseTuple(item, "ff", &x, &y))
      return nullptr;
    points_collecting.push_back({x, y});
  }
  const std::vector<Vec2f> points = points_collecting;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawPolyline(points, color, closed, thickness);
  Py_RETURN_NONE;
}

static PyObject* draw_convex_poly_filled(PyObject* self, PyObject* args)
{
  PyObject* points_list_obj;
  u32 color;
  if (!PyArg_ParseTuple(args, "O!I", &PyList_Type, &points_list_obj, &color))
    return nullptr;
  int num_points = PyList_Size(points_list_obj);
  if (num_points < 0)
    return nullptr;
  std::vector<Vec2f> points;
  for (int i = 0; i < num_points; ++i)
  {
    PyObject* item = PyList_GetItem(points_list_obj, i);
    float x, y;
    if (!PyArg_ParseTuple(item, "ff", &x, &y))
      return nullptr;
    points.push_back({x, y});
  }
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->DrawConvexPolyFilled(points, color);
  Py_RETURN_NONE;
}

// Retained-mode widgets. The owner is the current backend so a script's windows
// are pruned together when it stops; ids are opaque handles into the Gui tree.
static void* CurrentOwner()
{
  return PyScripting::PyScriptingBackend::GetCurrent();
}

static u64 widget_window(PyObject* self, const char* title, int embedded)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->GetOrCreateWindow(CurrentOwner(), std::string(title), embedded != 0);
}

static u64 widget_button(PyObject* self, u64 parent, const char* label)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->AddChild(parent, API::Gui::WidgetKind::Button, std::string(label));
}

static u64 widget_slider_float(PyObject* self, u64 parent, const char* label, float min, float max)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  u64 id = state->gui->AddChild(parent, API::Gui::WidgetKind::SliderFloat, std::string(label));
  state->gui->SetSliderRange(id, min, max);
  return id;
}

static u64 widget_text(PyObject* self, u64 parent, const char* text)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->AddChild(parent, API::Gui::WidgetKind::Text, std::string(text));
}

static u64 widget_checkbox(PyObject* self, u64 parent, const char* label, int checked)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  u64 id = state->gui->AddChild(parent, API::Gui::WidgetKind::Checkbox, std::string(label));
  state->gui->SetChecked(id, checked != 0);
  return id;
}

static u64 widget_input_text(PyObject* self, u64 parent, const char* label, const char* initial)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  u64 id = state->gui->AddChild(parent, API::Gui::WidgetKind::InputText, std::string(label));
  state->gui->SetInputText(id, std::string(initial));
  return id;
}

static bool widget_get_checked(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->GetChecked(id);
}

static void widget_set_checked(PyObject* self, u64 id, int checked)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetChecked(id, checked != 0);
}

static PyObject* widget_get_input_text(PyObject* self, PyObject* args)
{
  unsigned long long id;
  if (!PyArg_ParseTuple(args, "K", &id))
    return nullptr;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return PyUnicode_FromString(state->gui->GetInputText(id).c_str());
}

static void widget_set_input_text(PyObject* self, u64 id, const char* text)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetInputText(id, std::string(text));
}

static bool widget_take_clicked(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->TakeClicked(id);
}

static float widget_get_value(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->GetValue(id);
}

static void widget_set_value(PyObject* self, u64 id, float value)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetValue(id, value);
}

static void widget_set_text(PyObject* self, u64 id, const char* text)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetText(id, std::string(text));
}

static void SetupGuiModule(PyObject* module, GuiModuleState* state)
{
  static const char pycode[] = R"(
def add_osd_message(message: str, duration_ms: int = 2000, color_argb: int = 0xFFFFFF30):
    return _add_osd_message(message, duration_ms, color_argb)

def draw_line(a, b, color, thickness = 1):
    _draw_line(a[0], a[1], b[0], b[1], color, thickness)

def draw_rect(a, b, color, rounding = 0, thickness = 1):
    _draw_rect(a[0], a[1], b[0], b[1], color, rounding, thickness)

def draw_rect_filled(a, b, color, rounding= 0):
    _draw_rect_filled(a[0], a[1], b[0], b[1], color, rounding)

def draw_quad(a, b, c, d, color, thickness = 1):
    _draw_quad(a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1], color, thickness)

def draw_quad_filled(a, b, c, d, color):
    _draw_quad_filled(a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1], color)

def draw_triangle(a, b, c, color, thickness = 1):
    _draw_triangle(a[0], a[1], b[0], b[1], c[0], c[1], color, thickness)

def draw_triangle_filled(a, b, c, color):
    _draw_triangle_filled(a[0], a[1], b[0], b[1], c[0], c[1], color)

def draw_circle(center, radius, color, num_segments = None, thickness = 1):
    if num_segments is None:
        num_segments = 8 + int(radius // 50)
    _draw_circle(center[0], center[1], radius, color, num_segments, thickness)

def draw_circle_filled(center, radius, color, num_segments = None):
    if num_segments is None:
        num_segments = 8 + int(radius // 50)
    _draw_circle_filled(center[0], center[1], radius, color, num_segments)

def draw_text(pos, color, text):
    _draw_text(pos[0], pos[1], color, text)

def draw_polyline(points, color, closed = False, thickness = 1):
    _draw_polyline(points, color, closed, thickness)

def draw_convex_poly_filled(points, color):
    _draw_convex_poly_filled(points, color)

class Button:
    def __init__(self, id):
        self._id = id
    @property
    def clicked(self):
        return _widget_take_clicked(self._id)

class SliderFloat:
    def __init__(self, id):
        self._id = id
    @property
    def value(self):
        return _widget_get_value(self._id)
    @value.setter
    def value(self, v):
        _widget_set_value(self._id, v)

class Text:
    def __init__(self, id):
        self._id = id
    def set(self, text):
        _widget_set_text(self._id, text)

class Checkbox:
    def __init__(self, id):
        self._id = id
    @property
    def checked(self):
        return _widget_get_checked(self._id)
    @checked.setter
    def checked(self, v):
        _widget_set_checked(self._id, int(v))

class InputText:
    def __init__(self, id):
        self._id = id
    @property
    def value(self):
        return _widget_get_input_text(self._id)
    @value.setter
    def value(self, v):
        _widget_set_input_text(self._id, v)

class Window:
    def __init__(self, title, embedded=True):
        self._id = _widget_window(title, int(embedded))
    def button(self, label):
        return Button(_widget_button(self._id, label))
    def slider_float(self, label, min = 0.0, max = 1.0):
        return SliderFloat(_widget_slider_float(self._id, label, min, max))
    def text(self, text = ""):
        return Text(_widget_text(self._id, text))
    def checkbox(self, label, checked = False):
        return Checkbox(_widget_checkbox(self._id, label, int(checked)))
    def input_text(self, label, initial = ""):
        return InputText(_widget_input_text(self._id, label, initial))

def window(title, embedded=True):
    return Window(title, embedded)
)";
  Py::Object result = Py::LoadPyCodeIntoModule(module, pycode);
  if (result.IsNull())
  {
    ERROR_LOG_FMT(SCRIPTING, "Failed to load embedded python code into gui module");
  }
  API::Gui* gui = PyScripting::PyScriptingBackend::GetCurrent()->GetGui();
  state->gui = gui;
}

PyMODINIT_FUNC PyInit_gui()
{
  static PyMethodDef methods[] = {
      {"_add_osd_message", Py::as_py_func<add_osd_message>, METH_VARARGS, ""},
      {"clear_osd_messages", Py::as_py_func<clear_osd_messages>, METH_VARARGS, ""},
      {"get_display_size", get_display_size, METH_NOARGS, ""},
      {"_draw_line", Py::as_py_func<draw_line>, METH_VARARGS, ""},
      {"_draw_rect", Py::as_py_func<draw_rect>, METH_VARARGS, ""},
      {"_draw_rect_filled", Py::as_py_func<draw_rect_filled>, METH_VARARGS, ""},
      {"_draw_quad", Py::as_py_func<draw_quad>, METH_VARARGS, ""},
      {"_draw_quad_filled", Py::as_py_func<draw_quad_filled>, METH_VARARGS, ""},
      {"_draw_triangle", Py::as_py_func<draw_triangle>, METH_VARARGS, ""},
      {"_draw_triangle_filled", Py::as_py_func<draw_triangle_filled>, METH_VARARGS, ""},
      {"_draw_circle", Py::as_py_func<draw_circle>, METH_VARARGS, ""},
      {"_draw_circle_filled", Py::as_py_func<draw_circle_filled>, METH_VARARGS, ""},
      {"_draw_text", Py::as_py_func<draw_text>, METH_VARARGS, ""},
      {"_draw_polyline", draw_polyline, METH_VARARGS, ""},
      {"_draw_convex_poly_filled", draw_convex_poly_filled, METH_VARARGS, ""},
      {"_widget_window", Py::as_py_func<widget_window>, METH_VARARGS, ""},
      {"_widget_button", Py::as_py_func<widget_button>, METH_VARARGS, ""},
      {"_widget_slider_float", Py::as_py_func<widget_slider_float>, METH_VARARGS, ""},
      {"_widget_text", Py::as_py_func<widget_text>, METH_VARARGS, ""},
      {"_widget_checkbox", Py::as_py_func<widget_checkbox>, METH_VARARGS, ""},
      {"_widget_input_text", Py::as_py_func<widget_input_text>, METH_VARARGS, ""},
      {"_widget_get_checked", Py::as_py_func<widget_get_checked>, METH_VARARGS, ""},
      {"_widget_set_checked", Py::as_py_func<widget_set_checked>, METH_VARARGS, ""},
      {"_widget_get_input_text", widget_get_input_text, METH_VARARGS, ""},
      {"_widget_set_input_text", Py::as_py_func<widget_set_input_text>, METH_VARARGS, ""},
      {"_widget_take_clicked", Py::as_py_func<widget_take_clicked>, METH_VARARGS, ""},
      {"_widget_get_value", Py::as_py_func<widget_get_value>, METH_VARARGS, ""},
      {"_widget_set_value", Py::as_py_func<widget_set_value>, METH_VARARGS, ""},
      {"_widget_set_text", Py::as_py_func<widget_set_text>, METH_VARARGS, ""},

      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<GuiModuleState, SetupGuiModule>("gui", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
