#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"

#include <cstdlib>
#include <string>
#include <imgui.h>
#include <implot.h>

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::GraphicsAPI
{
const char* class_name = "GraphicsAPI";
static std::array all_graphics_functions_metadata_list = {
    FunctionMetadata("drawLine", "1.0", "drawLine(40.3, 80, 60.3, 80, 0.8, lineColorString)",
                     DrawLine, ArgTypeEnum::VoidType,
                     {ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::Float,
                      ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::String}),
    FunctionMetadata("drawEmptyRectangle", "1.0",
                     "drawEmptyRectangle(20.5, 45.8, 100.4, 75.9, 3.0, lineColorString)",
                     DrawEmptyRectangle, ArgTypeEnum::VoidType,
                     {ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::Float,
                      ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::String}),
    FunctionMetadata("drawFilledRectangle", "1.0",
                     "drawFilledRectangle(20.5, 45.8, 200.4, 75.9, fillColorString)",
                     DrawFilledRectangle, ArgTypeEnum::VoidType, {ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::Float, ArgTypeEnum::String})};



static bool IsEqualIgnoreCase(const char* string_1, const char* string_2)
{
  int index = 0;
  while (string_1[index] != '\0')
  {
    if (string_1[index] != string_2[index])
    {
      char first_char = string_1[index];
      if (first_char >= 65 && first_char <= 90)
        first_char += 32;
      char second_char = string_2[index];
      if (second_char >= 65 && second_char <= 90)
        second_char += 32;
      if (first_char != second_char)
        return false;
    }
    ++index;
  }

  return string_2[index] == '\0';
}

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
  else if (IsEqualIgnoreCase(color_string, "red"))
    return ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
  else if (IsEqualIgnoreCase(color_string, "green"))
    return ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
  else if (IsEqualIgnoreCase(color_string, "blue"))
    return ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
  else if (IsEqualIgnoreCase(color_string, "purple"))
    return ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 1.0f, 1.0f));
  else if (IsEqualIgnoreCase(color_string, "yellow"))
    return ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 1.f));
  else if (IsEqualIgnoreCase(color_string, "turquoise"))
    return ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
  else
    return 0x00000000;
}

ClassMetadata GetGraphicsApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_graphics_functions_metadata_list,
                                                   api_version, deprecated_functions_map)};
}

ArgHolder DrawLine(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{ 
  float x_coord_1 = args_list[0].float_val;
  float y_coord_1 = args_list[1].float_val;
  float x_coord_2 = args_list[2].float_val;
  float y_coord_2 = args_list[3].float_val;
  float thickness = args_list[4].float_val;

  std::string color = args_list[5].string_val;
  ImGui::GetForegroundDrawList()->AddLine({x_coord_1, y_coord_1}, {x_coord_2, y_coord_2},
                                          ParseColor(color.c_str()), thickness);
  return CreateVoidTypeArgHolder();

}

ArgHolder DrawEmptyRectangle(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  float bottom_left_x = args_list[0].float_val;
  float bottom_left_y = args_list[1].float_val;
  float top_right_x = args_list[2].float_val;
  float top_right_y = args_list[3].float_val;
  float thickness = args_list[4].float_val;
  std::string outline_color = args_list[5].string_val;

  ImGui::GetForegroundDrawList()->AddRect({bottom_left_x, bottom_left_y},
                                          {top_right_x, top_right_y}, ParseColor(outline_color.c_str()),
      0.0, 0, thickness);

  return CreateVoidTypeArgHolder();
}

ArgHolder DrawFilledRectangle(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  float bottom_left_x = args_list[0].float_val;
  float bottom_left_y = args_list[1].float_val;
  float top_right_x = args_list[2].float_val;
  float top_right_y = args_list[3].float_val;
  std::string fill_color = args_list[4].string_val;

  ImGui::GetForegroundDrawList()->AddRectFilled({bottom_left_x, bottom_left_y},
                                                {top_right_x, top_right_y},
                                                ParseColor(fill_color.c_str()), 0.0, 0);

  return CreateVoidTypeArgHolder();
}
}
