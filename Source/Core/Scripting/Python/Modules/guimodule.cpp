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

static void widget_enable_canvas(PyObject* self, u64 id, int width, int height)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->EnableCanvas(id, width, height);
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

static void widget_set_text_color(PyObject* self, u64 id, u32 color)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetTextColor(id, color);
}

static void widget_set_bg_color(PyObject* self, u64 id, u32 color)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetBgColor(id, color);
}

static void widget_set_style(PyObject* self, u64 id, const char* qss)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->SetStyle(id, std::string(qss));
}

static u64 canvas_window(PyObject* self, const char* title, int width, int height, int embedded,
                         int overlay)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->GetOrCreateCanvas(CurrentOwner(), std::string(title), width, height,
                                       embedded != 0, overlay != 0);
}

static void canvas_clear(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->CanvasClear(id);
}

static void canvas_commit(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->CanvasCommit(id);
}

static void canvas_line(PyObject* self, u64 id, float ax, float ay, float bx, float by, u32 color,
                        float thickness)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->CanvasAdd(id, P{P::Type::Line, {ax, ay}, {bx, by}, {}, 0.0f, thickness, 0.0f, color});
}

static void canvas_rect(PyObject* self, u64 id, float ax, float ay, float bx, float by, u32 color,
                        int filled, float rounding, float thickness)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  const auto type = filled ? P::Type::RectFilled : P::Type::Rect;
  state->gui->CanvasAdd(id, P{type, {ax, ay}, {bx, by}, {}, 0.0f, thickness, rounding, color});
}

static void canvas_circle(PyObject* self, u64 id, float cx, float cy, float radius, u32 color,
                          int filled, float thickness)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  const auto type = filled ? P::Type::CircleFilled : P::Type::Circle;
  state->gui->CanvasAdd(id, P{type, {cx, cy}, {}, {}, radius, thickness, 0.0f, color});
}

static void canvas_triangle(PyObject* self, u64 id, float ax, float ay, float bx, float by,
                            float cx, float cy, u32 color, int filled, float thickness)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  const auto type = filled ? P::Type::TriangleFilled : P::Type::Triangle;
  state->gui->CanvasAdd(id,
                        P{type, {ax, ay}, {bx, by}, {cx, cy}, 0.0f, thickness, 0.0f, color});
}

static void canvas_text(PyObject* self, u64 id, float x, float y, u32 color, const char* text)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->CanvasAdd(id, P{P::Type::Text, {x, y}, {}, {}, 0.0f, 1.0f, 0.0f, color,
                              std::string(text)});
}

static void canvas_image(PyObject* self, u64 id, const char* path, float x, float y, float w,
                         float h, u32 tint, float sx0, float sy0, float sx1, float sy1)
{
  using P = API::Gui::CanvasPrimitive;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  state->gui->CanvasAdd(id, P{P::Type::Image, {x, y}, {x + w, y + h}, {}, 0.0f, 1.0f, 0.0f, tint,
                              std::string{}, std::string(path), {sx0, sy0}, {sx1, sy1}});
}

// Returns (x, y, inside) for the canvas cursor; coords are canvas pixels.
static PyObject* canvas_mouse_pos(PyObject* self, PyObject* args)
{
  u64 id;
  if (!PyArg_ParseTuple(args, "K", &id))
    return nullptr;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  bool inside = false;
  const auto pos = state->gui->CanvasMousePos(id, inside);
  return Py_BuildValue("(ffO)", pos.x, pos.y, inside ? Py_True : Py_False);
}

// Returns (x, y) of the last unconsumed left-click, or None. Consumes it.
static PyObject* canvas_take_click(PyObject* self, PyObject* args)
{
  u64 id;
  if (!PyArg_ParseTuple(args, "K", &id))
    return nullptr;
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  Vec2f pos;
  if (!state->gui->CanvasTakeClick(id, pos))
    Py_RETURN_NONE;
  return Py_BuildValue("(ff)", pos.x, pos.y);
}

// Returns accumulated wheel notches since last call (positive = scroll up). Consumes it.
static float canvas_take_wheel(PyObject* self, u64 id)
{
  GuiModuleState* state = Py::GetState<GuiModuleState>(self);
  return state->gui->CanvasTakeWheel(id);
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

class _BaseWindow:
    def __init__(self, wid):
        self._id = wid
    def canvas(self, width, height):
        # Attach a drawing surface to this window; form widgets added here sit below it.
        _widget_enable_canvas(self._id, width, height)
        return Canvas(self._id, width, height)
    def _child(self, wid, style, text_color, bg_color):
        if text_color is not None:
            _widget_set_text_color(wid, text_color)
        if bg_color is not None:
            _widget_set_bg_color(wid, bg_color)
        if style is not None:
            _widget_set_style(wid, style)
        return wid

class Overlay(_BaseWindow):
    def button(self, label, *, text_color=None, bg_color=None):
        return Button(self._child(_widget_button(self._id, label), None, text_color, bg_color))
    def slider_float(self, label, min = 0.0, max = 1.0, *, text_color=None, bg_color=None):
        return SliderFloat(self._child(_widget_slider_float(self._id, label, min, max),
                                       None, text_color, bg_color))
    def text(self, text = "", *, text_color=None, bg_color=None):
        return Text(self._child(_widget_text(self._id, text), None, text_color, bg_color))
    def checkbox(self, label, checked = False, *, text_color=None, bg_color=None):
        return Checkbox(self._child(_widget_checkbox(self._id, label, int(checked)),
                                    None, text_color, bg_color))
    def input_text(self, label, initial = "", *, text_color=None, bg_color=None):
        return InputText(self._child(_widget_input_text(self._id, label, initial),
                                     None, text_color, bg_color))

class Window(_BaseWindow):
    def button(self, label, *, style=None, text_color=None, bg_color=None):
        return Button(self._child(_widget_button(self._id, label), style, text_color, bg_color))
    def slider_float(self, label, min = 0.0, max = 1.0, *, style=None, text_color=None, bg_color=None):
        return SliderFloat(self._child(_widget_slider_float(self._id, label, min, max),
                                       style, text_color, bg_color))
    def text(self, text = "", *, style=None, text_color=None, bg_color=None):
        return Text(self._child(_widget_text(self._id, text), style, text_color, bg_color))
    def checkbox(self, label, checked = False, *, style=None, text_color=None, bg_color=None):
        return Checkbox(self._child(_widget_checkbox(self._id, label, int(checked)),
                                    style, text_color, bg_color))
    def input_text(self, label, initial = "", *, style=None, text_color=None, bg_color=None):
        return InputText(self._child(_widget_input_text(self._id, label, initial),
                                     style, text_color, bg_color))

class Canvas:
    def __init__(self, id, width, height):
        self._id = id
        self.width = width
        self.height = height
    def clear(self):
        _canvas_clear(self._id)
    def commit(self):
        _canvas_commit(self._id)
    def line(self, a, b, color, thickness = 1):
        _canvas_line(self._id, a[0], a[1], b[0], b[1], color, thickness)
    def rect(self, a, b, color, rounding = 0, thickness = 1):
        _canvas_rect(self._id, a[0], a[1], b[0], b[1], color, 0, rounding, thickness)
    def rect_filled(self, a, b, color, rounding = 0):
        _canvas_rect(self._id, a[0], a[1], b[0], b[1], color, 1, rounding, 1)
    def circle(self, center, radius, color, thickness = 1):
        _canvas_circle(self._id, center[0], center[1], radius, color, 0, thickness)
    def circle_filled(self, center, radius, color):
        _canvas_circle(self._id, center[0], center[1], radius, color, 1, 1)
    def triangle(self, a, b, c, color, thickness = 1):
        _canvas_triangle(self._id, a[0], a[1], b[0], b[1], c[0], c[1], color, 0, thickness)
    def triangle_filled(self, a, b, c, color):
        _canvas_triangle(self._id, a[0], a[1], b[0], b[1], c[0], c[1], color, 1, 1)
    def text(self, pos, color, text):
        _canvas_text(self._id, pos[0], pos[1], color, text)
    def image(self, path, pos, size, *, tint=0, src=(0.0, 0.0, 1.0, 1.0)):
        # tint=0 draws the texture as-is; otherwise RGB tints it and alpha scales opacity.
        # src is a normalized (x0, y0, x1, y1) crop for partial draws (e.g. analog fills).
        _canvas_image(self._id, path, pos[0], pos[1], size[0], size[1], tint,
                      src[0], src[1], src[2], src[3])
    def mouse_pos(self):
        # (x, y, inside): last cursor position in canvas pixels; inside is False off-widget.
        return _canvas_mouse_pos(self._id)
    def take_click(self):
        # (x, y) of the last unconsumed left-click in canvas pixels, or None. Consumes it.
        return _canvas_take_click(self._id)
    def take_wheel(self):
        # Accumulated wheel notches since the last call (positive = scroll up). Consumes it.
        return _canvas_take_wheel(self._id)

def canvas(title, width, height, *, embedded=False, overlay=False):
    return Canvas(_canvas_window(title, width, height, int(embedded), int(overlay)), width, height)

def overlay(title, *, bg_color=None, text_color=None):
    w = Overlay(_widget_window(title, 1))
    if bg_color is not None:
        _widget_set_bg_color(w._id, bg_color)
    if text_color is not None:
        _widget_set_text_color(w._id, text_color)
    return w

def window(title, *, style=None, bg_color=None, text_color=None):
    w = Window(_widget_window(title, 0))
    if bg_color is not None:
        _widget_set_bg_color(w._id, bg_color)
    if text_color is not None:
        _widget_set_text_color(w._id, text_color)
    if style is not None:
        _widget_set_style(w._id, style)
    return w
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
      {"_widget_enable_canvas", Py::as_py_func<widget_enable_canvas>, METH_VARARGS, ""},
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
      {"_widget_set_text_color", Py::as_py_func<widget_set_text_color>, METH_VARARGS, ""},
      {"_widget_set_bg_color", Py::as_py_func<widget_set_bg_color>, METH_VARARGS, ""},
      {"_widget_set_style", Py::as_py_func<widget_set_style>, METH_VARARGS, ""},
      {"_canvas_window", Py::as_py_func<canvas_window>, METH_VARARGS, ""},
      {"_canvas_clear", Py::as_py_func<canvas_clear>, METH_VARARGS, ""},
      {"_canvas_commit", Py::as_py_func<canvas_commit>, METH_VARARGS, ""},
      {"_canvas_line", Py::as_py_func<canvas_line>, METH_VARARGS, ""},
      {"_canvas_rect", Py::as_py_func<canvas_rect>, METH_VARARGS, ""},
      {"_canvas_circle", Py::as_py_func<canvas_circle>, METH_VARARGS, ""},
      {"_canvas_triangle", Py::as_py_func<canvas_triangle>, METH_VARARGS, ""},
      {"_canvas_text", Py::as_py_func<canvas_text>, METH_VARARGS, ""},
      {"_canvas_image", Py::as_py_func<canvas_image>, METH_VARARGS, ""},
      {"_canvas_mouse_pos", canvas_mouse_pos, METH_VARARGS, ""},
      {"_canvas_take_click", canvas_take_click, METH_VARARGS, ""},
      {"_canvas_take_wheel", Py::as_py_func<canvas_take_wheel>, METH_VARARGS, ""},

      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<GuiModuleState, SetupGuiModule>("gui", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
