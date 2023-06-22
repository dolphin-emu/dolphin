#pragma once
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"
namespace Scripting::GraphicsAPI
{

extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* DrawLine(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawEmptyRectangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawFilledRectangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawEmptyTriangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawFilledTriangle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawEmptyCircle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawFilledCircle(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawEmptyPolygon(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawFilledPolygon(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* DrawText(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* AddCheckbox(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetCheckboxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* SetCheckboxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* AddRadioButtonGroup(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* AddRadioButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetRadioButtonGroupValue(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list);
ArgHolder* SetRadioButtonGroupValue(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list);
ArgHolder* AddTextBox(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetTextBoxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* SetTextBoxValue(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* AddButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* PressButton(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* NewLine(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BeginWindow(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* EndWindow(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
}  // namespace Scripting::GraphicsAPI
