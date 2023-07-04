#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"

#include <atomic>
#include <cstdlib>
#include <deque>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <stack>
#include <string>

#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/Scripting/ScriptUtilities.h"

namespace Scripting::GraphicsAPI
{

std::stack<bool> display_stack = std::stack<bool>();
bool window_is_open = false;
const char* class_name = "GraphicsAPI";

static std::deque<bool> all_checkboxes = std::deque<bool>();
static std::map<long long, bool*> id_to_checkbox_map = std::map<long long, bool*>();

static std::deque<int> all_radio_groups = std::deque<int>();
static std::map<long long, int*> id_to_radio_group_map = std::map<long long, int*>();

static std::deque<std::string> all_text_boxes = std::deque<std::string>();
static std::map<long long, std::string*> id_to_text_box_map = std::map<long long, std::string*>();

static std::array all_graphics_functions_metadata_list = {
    FunctionMetadata("drawLine", "1.0", "drawLine(40.3, 80, 60.3, 80, 0.8, lineColorString)",
                     DrawLine, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawEmptyRectangle", "1.0",
                     "drawEmptyRectangle(20.5, 45.8, 100.4, 75.9, 3.0, lineColorString)",
                     DrawEmptyRectangle, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawFilledRectangle", "1.0",
                     "drawFilledRectangle(20.5, 45.8, 200.4, 75.9, fillColorString)",
                     DrawFilledRectangle, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawEmptyTriangle", "1.0",
                     "drawEmptyTriangle(x1, y1, x2, y2, x3, y3, thickness, color)",
                     DrawEmptyTriangle, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawFilledTriangle", "1.0",
                     "drawFilledTriangle(x1, y1, x2, y2, x3, y3, color)", DrawFilledTriangle,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawEmptyCircle", "1.0",
                     "drawEmptyCircle(centerX, centerY, radius, thickness, colorString)",
                     DrawEmptyCircle, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawFilledCircle", "1.0",
                     "drawFillecCircle(centerX, centerY, radius, fillColorString)",
                     DrawFilledCircle, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawEmptyPolygon", "1.0",
                     "drawEmptyPolygon({ {45.0, 100.0}, {45.0, 500.0}, {67.4, 54.2}}, 12.3, color)",
                     DrawEmptyPolygon, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::ListOfPoints, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata(
        "drawFilledPolygon", "1.0",
        "drawFilledPolygon({ {45.0, 100.0}, {45.0, 500.0}, {67.4, 54.2}}, color)",
        DrawFilledPolygon, ScriptingEnums::ArgTypeEnum::VoidType,
        {ScriptingEnums::ArgTypeEnum::ListOfPoints, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("drawText", "1.0", "drawText(30.0, 45.0, colorString, \"Hello World!\")",
                     DrawText, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("addCheckbox", "1.0", "addCheckbox(checkboxLabel, 42)", AddCheckbox,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("getCheckboxValue", "1.0", "getCheckboxValue(42)", GetCheckboxValue,
                     ScriptingEnums::ArgTypeEnum::Boolean, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("setCheckboxValue", "1.0", "setCheckboxValue(42, true)", SetCheckboxValue,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::Boolean}),

    FunctionMetadata("addRadioButtonGroup", "1.0", "addRadioButtonGroup(42)", AddRadioButtonGroup,
                     ScriptingEnums::ArgTypeEnum::VoidType, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("addRadioButton", "1.0", "addRadioButton(\"apples\", 42, 0)", AddRadioButton,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::S64,
                      ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("getRadioButtonGroupValue", "1.0", "getRadioButtonGroupValue((42)",
                     GetRadioButtonGroupValue, ScriptingEnums::ArgTypeEnum::S64,
                     {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("setRadioButtonGroupValue", "1.0", "setRadioButtonGroupValue(42, 1)",
                     SetRadioButtonGroupValue, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::S64}),

    FunctionMetadata("addTextBox", "1.0", "addTextBox(42, \"Textbox Label\")", AddTextBox,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getTextBoxValue", "1.0", "getTextBoxValue(42)", GetTextBoxValue,
                     ScriptingEnums::ArgTypeEnum::String, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("setTextBoxValue", "1.0", "setTextBoxValue(42, \"Hello World!\")",
                     SetTextBoxValue, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::S64, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("addButton", "1.0",
                     "addButton(\"Button Label\", 42, callbackFunc, 100.0, 45.0)", AddButton,
                     ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::S64,
                      ScriptingEnums::ArgTypeEnum::RegistrationForButtonCallbackInputType,
                      ScriptingEnums::ArgTypeEnum::Float, ScriptingEnums::ArgTypeEnum::Float}),
    FunctionMetadata("pressButton", "1.0", "pressButton(42)", PressButton,
                     ScriptingEnums::ArgTypeEnum::VoidType, {ScriptingEnums::ArgTypeEnum::S64}),

    FunctionMetadata("newLine", "1.0", "newLine(10.0)", NewLine,
                     ScriptingEnums::ArgTypeEnum::VoidType, {ScriptingEnums::ArgTypeEnum::Float}),

    FunctionMetadata("beginWindow", "1.0", "beginWindow(windowName)", BeginWindow,
                     ScriptingEnums::ArgTypeEnum::VoidType, {ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("endWindow", "1.0", "endWindow()", EndWindow,
                     ScriptingEnums::ArgTypeEnum::VoidType, {})};

u32 ParseColor(const char* color_string)
{
  char* x = nullptr;
  if (color_string == nullptr || color_string[0] == '\0')
    return 0x00000000;

  int index_after_x = -1;
  if (color_string[0] == '0' && (color_string[1] == 'x' || color_string[1] == 'X'))
    index_after_x = 2;
  else if (color_string[0] == 'x' || color_string[0] == 'X')
    index_after_x = 1;

  if (index_after_x != -1)
  {
    std::string string_after_x = std::string(color_string).substr(index_after_x);
    int string_after_x_length = static_cast<int>(string_after_x.length());
    u8 red_int = 0;
    u8 green_int = 0;
    u8 blue_int = 0;
    u8 brightness_int = 255;

    if (string_after_x_length >= 2)
    {
      std::string red_string = string_after_x.substr(0, 2);
      red_int = strtoul(red_string.c_str(), &x, 16);
    }
    if (string_after_x_length >= 4)
    {
      std::string green_string = string_after_x.substr(2, 2);
      green_int = strtoul(green_string.c_str(), &x, 16);
    }

    if (string_after_x_length >= 6)
    {
      std::string blue_string = string_after_x.substr(4, 2);
      blue_int = strtoul(blue_string.c_str(), &x, 16);
    }

    if (string_after_x_length >= 8)
    {
      std::string brightness_string = string_after_x.substr(6, 2);
      brightness_int = strtoul(brightness_string.c_str(), &x, 16);
    }

    float red_float = 0.0f;
    float green_float = 0.0f;
    float blue_float = 0.0f;
    float brightness_float = 1.0f;

    if (red_int == 0)
      red_float = 0.0f;
    else if (red_int == 255)
      red_float = 1.0f;
    else
      red_float = (1.0 * red_int) / 255.0;

    if (green_int == 0)
      green_float = 0.0f;
    else if (green_int == 255)
      green_float = 1.0f;
    else
      green_float = (1.0 * green_int) / 255.0;

    if (blue_int == 0)
      blue_float = 0.0f;
    else if (blue_int == 255)
      blue_float = 1.0f;
    else
      blue_float = (1.0 * blue_int) / 255.0;

    if (brightness_int == 0)
      brightness_float = 0.0f;
    else if (brightness_int == 255)
      brightness_float = 1.0f;
    else
      brightness_float = (1.0 * brightness_int) / 255.0;

    return ImGui::GetColorU32(ImVec4(red_float, green_float, blue_float, brightness_float));
  }
  return 0x000000ff;
}

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_graphics_functions_metadata_list,
                                                   api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_graphics_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_graphics_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder* DrawLine(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float x_coord_1 = (*args_list)[0]->float_val;
  float y_coord_1 = (*args_list)[1]->float_val;
  float x_coord_2 = (*args_list)[2]->float_val;
  float y_coord_2 = (*args_list)[3]->float_val;
  float thickness = (*args_list)[4]->float_val;
  std::string color = (*args_list)[5]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddLine({window_edge.x + x_coord_1, window_edge.y + y_coord_1},
                     {window_edge.x + x_coord_2, window_edge.y + y_coord_2},
                     ParseColor(color.c_str()), thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawEmptyRectangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float bottom_left_x = (*args_list)[0]->float_val;
  float bottom_left_y = (*args_list)[1]->float_val;
  float top_right_x = (*args_list)[2]->float_val;
  float top_right_y = (*args_list)[3]->float_val;
  float thickness = (*args_list)[4]->float_val;
  std::string outline_color = (*args_list)[5]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddRect({window_edge.x + bottom_left_x, window_edge.y + bottom_left_y},
                     {window_edge.x + top_right_x, window_edge.y + top_right_y},
                     ParseColor(outline_color.c_str()), 0.0, 0, thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawFilledRectangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float bottom_left_x = (*args_list)[0]->float_val;
  float bottom_left_y = (*args_list)[1]->float_val;
  float top_right_x = (*args_list)[2]->float_val;
  float top_right_y = (*args_list)[3]->float_val;
  std::string fill_color = (*args_list)[4]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddRectFilled({window_edge.x + bottom_left_x, window_edge.y + bottom_left_y},
                           {window_edge.x + top_right_x, window_edge.y + top_right_y},
                           ParseColor(fill_color.c_str()), 0.0, 0);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawEmptyTriangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float x1 = (*args_list)[0]->float_val;
  float y1 = (*args_list)[1]->float_val;
  float x2 = (*args_list)[2]->float_val;
  float y2 = (*args_list)[3]->float_val;
  float x3 = (*args_list)[4]->float_val;
  float y3 = (*args_list)[5]->float_val;
  float thickness = (*args_list)[6]->float_val;
  std::string color = (*args_list)[7]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddTriangle(
      {window_edge.x + x1, window_edge.y + y1}, {window_edge.x + x2, window_edge.y + y2},
      {window_edge.x + x3, window_edge.y + y3}, ParseColor(color.c_str()), thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawFilledTriangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float x1 = (*args_list)[0]->float_val;
  float y1 = (*args_list)[1]->float_val;
  float x2 = (*args_list)[2]->float_val;
  float y2 = (*args_list)[3]->float_val;
  float x3 = (*args_list)[4]->float_val;
  float y3 = (*args_list)[5]->float_val;
  std::string fill_color = (*args_list)[6]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddTriangleFilled(
      {window_edge.x + x1, window_edge.y + y1}, {window_edge.x + x2, window_edge.y + y2},
      {window_edge.x + x3, window_edge.y + y3}, ParseColor(fill_color.c_str()));

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawEmptyCircle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float centerX = (*args_list)[0]->float_val;
  float centerY = (*args_list)[1]->float_val;
  float radius = (*args_list)[2]->float_val;
  float thickness = (*args_list)[3]->float_val;
  std::string outline_color = (*args_list)[4]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddCircle({window_edge.x + centerX, window_edge.y + centerY}, radius,
                       ParseColor(outline_color.c_str()), 0, thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawFilledCircle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float centerX = (*args_list)[0]->float_val;
  float centerY = (*args_list)[1]->float_val;
  float radius = (*args_list)[2]->float_val;
  std::string fill_color = (*args_list)[3]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddCircleFilled({window_edge.x + centerX, window_edge.y + centerY}, radius,
                             ParseColor(fill_color.c_str()), 0);

  return CreateVoidTypeArgHolder();
}

/*
ArgHolder DrawEmptyArc(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float x1 = (*args_list)[0]->float_val;
  float y1 = (*args_list)[1]->float_val;
  float x2 = (*args_list)[2]->float_val;
  float y2 = (*args_list)[3]->float_val;
  float x3 = (*args_list)[4]->float_val;
  float y3 = (*args_list)[5]->float_val;
  float x4 = (*args_list)[6]->float_val;
  float y4 = (*args_list)[7]->float_val;
  long long num_sides = (*args_list)[6]->s64_val;

  ImGui::GetForegroundDrawList()->AddBezierCubic({x1, y1}, {x2, y2}, {x3, y3},
                                                     {x4, y4} , ParseColor("yellow"), 5.0,
num_sides); return CreateVoidTypeArgHolder();
}
*/

ArgHolder* DrawEmptyPolygon(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::vector<ImVec2> list_of_points = (*args_list)[0]->list_of_points;
  float thickness = (*args_list)[1]->float_val;
  std::string line_color = (*args_list)[2]->string_val;

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  for (int i = 0; i < list_of_points.size(); ++i)
  {
    list_of_points[i] = {list_of_points[i].x + window_edge.x, list_of_points[i].y + window_edge.y};
  }

  ImDrawList* draw_list = nullptr;
  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  draw_list->AddPolyline(&list_of_points[0], (int)list_of_points.size(),
                         ParseColor(line_color.c_str()), ImDrawFlags_Closed, thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawFilledPolygon(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::vector<ImVec2> list_of_points = (*args_list)[0]->list_of_points;
  std::string line_color = (*args_list)[1]->string_val;

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  for (int i = 0; i < list_of_points.size(); ++i)
  {
    list_of_points[i] = {list_of_points[i].x + window_edge.x, list_of_points[i].y + window_edge.y};
  }

  ImDrawList* draw_list = nullptr;
  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  draw_list->AddConvexPolyFilled(&list_of_points[0], (int)list_of_points.size(),
                                 ParseColor(line_color.c_str()));

  return CreateVoidTypeArgHolder();
}

ArgHolder* DrawText(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  float x = (*args_list)[0]->float_val;
  float y = (*args_list)[1]->float_val;
  std::string color_string = (*args_list)[2]->string_val;
  std::string display_text = (*args_list)[3]->string_val;
  ImDrawList* draw_list = nullptr;

  if (!window_is_open)
    draw_list = ImGui::GetForegroundDrawList();
  else
  {
    if (display_stack.empty() || !display_stack.top())
      return CreateVoidTypeArgHolder();
    draw_list = ImGui::GetWindowDrawList();
  }

  ImVec2 window_edge = ImGui::GetCursorScreenPos();
  draw_list->AddText({window_edge.x + x, window_edge.y + y}, ParseColor(color_string.c_str()),
                     display_text.c_str(), nullptr);

  return CreateVoidTypeArgHolder();
}

ArgHolder* AddCheckbox(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::string checkbox_name = (*args_list)[0]->string_val;
  long long id = (*args_list)[1]->s64_val;

  if (!window_is_open)
  {
    return CreateErrorStringArgHolder(
        "Must have window open (using GraphicsAPI:beginWindow(\"winName\") before you can add a "
        "checkbox!");
  }
  else
  {
    if (id_to_checkbox_map.count(id) == 0)
    {
      all_checkboxes.push_back(false);
      id_to_checkbox_map[id] = &all_checkboxes[all_checkboxes.size() - 1];
    }

    if (!display_stack.empty() && display_stack.top())
      ImGui::Checkbox(checkbox_name.c_str(), id_to_checkbox_map[id]);
    return CreateVoidTypeArgHolder();
  }
}

ArgHolder* GetCheckboxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long checkbox_id = (*args_list)[0]->s64_val;
  if (id_to_checkbox_map.count(checkbox_id) == 0)
    return CreateErrorStringArgHolder(
        fmt::format("Attempted to get the value of an undefined checkbox with an index of {}. User "
                    "must call addCheckbox() before they can get the checkbox's value!",
                    checkbox_id));

  return CreateBoolArgHolder(*(id_to_checkbox_map[checkbox_id]));
}

ArgHolder* SetCheckboxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long checkbox_id = (*args_list)[0]->s64_val;
  bool new_bool_value = (*args_list)[1]->bool_val;
  if (id_to_checkbox_map.count(checkbox_id) == 0)
    return CreateErrorStringArgHolder(fmt::format(
        "Attempted to set the value of a checkbox with an index of {} before creating it. User "
        "must call addCheckbox() to create the checkbox before they can set its value!",
        checkbox_id));

  *(id_to_checkbox_map[checkbox_id]) = new_bool_value;

  return CreateVoidTypeArgHolder();
}

ArgHolder* AddRadioButtonGroup(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long id = (*args_list)[0]->s64_val;
  if (!window_is_open)
  {
    return CreateErrorStringArgHolder(
        "Must have window open (using GraphicsAPI:beginWindow(\"winName\") before you can add a "
        "RadioButtonGroup!");
  }
  else
  {
    if (id_to_radio_group_map.count(id) == 0)
    {
      all_radio_groups.push_back(0);
      id_to_radio_group_map[id] = &all_radio_groups[all_radio_groups.size() - 1];
    }
    return CreateVoidTypeArgHolder();
  }
}

ArgHolder* AddRadioButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::string button_name = (*args_list)[0]->string_val;
  long long radio_button_group_id = (*args_list)[1]->s64_val;
  long long radio_button_id = (*args_list)[2]->s64_val;
  if (!window_is_open)
  {
    return CreateErrorStringArgHolder(
        "Must have window open (using GraphicsAPI:beginWindow(\"winName\") before you can add a "
        "RadioButton!");
  }
  else if (id_to_radio_group_map.count(radio_button_group_id) == 0)
  {
    return CreateErrorStringArgHolder(
        fmt::format("Attempted to add radio button to an invalid group of {}. Please create the "
                    "Radio Button Group by calling GraphicsAPI:addRadioButtonGroup(idNumber) "
                    "before trying to pass IdNumber into this function for the radio group number!",
                    radio_button_group_id));
  }
  else if (!display_stack.empty() && display_stack.top())
    ImGui::RadioButton(button_name.c_str(), id_to_radio_group_map[radio_button_group_id],
                       radio_button_id);

  return CreateVoidTypeArgHolder();
}

ArgHolder* GetRadioButtonGroupValue(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list)
{
  long long radio_group_id = (*args_list)[0]->s64_val;
  if (id_to_radio_group_map.count(radio_group_id) == 0)
    return CreateErrorStringArgHolder(fmt::format(
        "Attempted to get the value of an undefined radio group with an ID of {}. User must call "
        "addRadioButtonGroup() before they can get the radio button's value!",
        radio_group_id));
  return CreateS64ArgHolder(*(id_to_radio_group_map[radio_group_id]));
}

ArgHolder* SetRadioButtonGroupValue(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list)
{
  long long radio_group_id = (*args_list)[0]->s64_val;
  long long new_int_value = (*args_list)[1]->s64_val;
  if (id_to_radio_group_map.count(radio_group_id) == 0)
    return CreateErrorStringArgHolder(
        fmt::format("Attempted to set the value of a radio group with an ID of {} before creating "
                    "it. User must call addRadioButtonGroup() before they can set the value of a "
                    "radio button group!",
                    radio_group_id));
  *(id_to_radio_group_map[radio_group_id]) = new_int_value;
  return CreateVoidTypeArgHolder();
}

ArgHolder* AddTextBox(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long text_box_id = (*args_list)[0]->s64_val;
  std::string text_box_name = (*args_list)[1]->string_val;

  if (!window_is_open)
  {
    return CreateErrorStringArgHolder(
        "Must have window open (using GraphicsAPI:beginWindow(\"winName\") before you can add a "
        "TextBox!");
  }
  else
  {
    if (id_to_text_box_map.count(text_box_id) == 0)
    {
      all_text_boxes.push_back("");
      id_to_text_box_map[text_box_id] = &(all_text_boxes[all_text_boxes.size() - 1]);
    }

    if (!display_stack.empty() && display_stack.top())
      ImGui::InputText(text_box_name.c_str(), id_to_text_box_map[text_box_id]);
  }
  return CreateVoidTypeArgHolder();
}

ArgHolder* GetTextBoxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long text_box_id = (*args_list)[0]->s64_val;
  if (id_to_text_box_map.count(text_box_id) == 0)
    return CreateErrorStringArgHolder(fmt::format(
        "Attempted to get the textbox value of an invalid textbox with an ID of {}. User must call "
        "addTextBox() to create a text box before they can get its value!",
        text_box_id));
  return CreateStringArgHolder(*(id_to_text_box_map[text_box_id]));
}

ArgHolder* SetTextBoxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long text_box_id = (*args_list)[0]->s64_val;
  std::string new_string_value = (*args_list)[1]->string_val;
  if (id_to_text_box_map.count(text_box_id) == 0)
    return CreateErrorStringArgHolder(
        "Attempted to set the value of a text box which had not been created. User "
        "must call addTextBox() to create a text box before they can set its value!");
  *(id_to_text_box_map[text_box_id]) = new_string_value;
  return CreateVoidTypeArgHolder();
}

ArgHolder* AddButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::string button_label = (*args_list)[0]->string_val;
  long long button_id = (*args_list)[1]->s64_val;
  void* function_callback = (*args_list)[2]->void_pointer_val;
  float button_width = (*args_list)[3]->float_val;
  float button_height = (*args_list)[4]->float_val;

  if (!window_is_open)
  {
    return CreateErrorStringArgHolder("Cannot add button directly to screen - must open a window "
                                      "first by calling GraphicsAPI:beginWindow()");
  }

  else if (!display_stack.empty() && display_stack.top())
  {
    current_script->dll_specific_api_definitions.RegisterButtonCallback(current_script, button_id,
                                                                        function_callback);
    if (ImGui::Button(button_label.c_str(), {button_width, button_height}))
    {  // true when button was pressed, and false otherwise
      current_script->dll_specific_api_definitions.GetButtonCallbackAndAddToQueue(current_script,
                                                                                  button_id);
    }
  }
  return CreateVoidTypeArgHolder();
}

ArgHolder* PressButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long button_id = (*args_list)[0]->s64_val;

  if (!window_is_open || display_stack.empty() || !display_stack.top())
  {
    return CreateErrorStringArgHolder("Cannot press button which is not displayed on screen!");
  }

  if (!current_script->dll_specific_api_definitions.IsButtonRegistered(current_script, button_id))
    return CreateErrorStringArgHolder(fmt::format(
        "Attempted to press undefined button of {}. User must call "
        "GraphicsAPI:registerButtonCallback() before they can call GraphicsAPI:pressButton()",
        button_id));

  current_script->dll_specific_api_definitions.GetButtonCallbackAndAddToQueue(current_script,
                                                                              button_id);

  return CreateVoidTypeArgHolder();
}

ArgHolder* NewLine(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!window_is_open)
    return CreateErrorStringArgHolder("Cannot add new line when no window is open on screen!");
  float vertical_offset = (*args_list)[0]->float_val;
  ImGui::Dummy(ImVec2(0.0f, vertical_offset));
  return CreateVoidTypeArgHolder();
}

ArgHolder* BeginWindow(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!window_is_open)
  {
    window_is_open = true;
    display_stack.push(ImGui::Begin((*args_list)[0]->string_val.c_str()));
  }
  else
  {
    display_stack.push(ImGui::TreeNode((*args_list)[0]->string_val.c_str()));
  }

  return CreateVoidTypeArgHolder();
}

ArgHolder* EndWindow(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!window_is_open)
    return CreateVoidTypeArgHolder();
  else
  {
    if (display_stack.empty())
      return CreateErrorStringArgHolder(
          "endWindow() was in an invalid internal state, with an empty list of windows to display "
          "but with 1 or more windows/treenodes open!");
    bool was_displayed = display_stack.top();
    display_stack.pop();
    if (display_stack.empty())
    {
      window_is_open = false;
      ImGui::End();
    }
    else if (was_displayed)
      ImGui::TreePop();
  }
  return CreateVoidTypeArgHolder();
}
}  // namespace Scripting::GraphicsAPI
