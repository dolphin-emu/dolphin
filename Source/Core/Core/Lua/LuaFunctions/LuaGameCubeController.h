#pragma once

#include <lua.hpp>

#include <Array>
#include <memory>
#include <vector>
#include <string>

#include "Core/HW/GCPad.h"
#include "Core/Movie.h"
#include "Core/Lua/LuaHelperClasses/LuaGameCubeButtonProbabilityClasses.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Lua::LuaGameCubeController
{

  extern std::array<bool, 4> overwriteControllerAtSpecifiedPort;
  extern std::array<bool, 4> addToControllerAtSpecifiedPort;
  extern std::array<bool, 4> doRandomInputEventsAtSpecifiedPort;


  extern std::array<Movie::ControllerState, 4> newOverwriteControllerInputs;
  extern std::array<Movie::ControllerState, 4> addToControllerInputs;
  extern std::array<std::vector<GcButtonName>, 4> buttonListsForAddToControllerInputs;
  extern std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4> randomButtonEvents;
  
  void InitLuaGameCubeControllerFunctions(lua_State* luaState, const std::string& luaApiVersion);

  int setInputs(lua_State* luaState);
  int addInputs(lua_State* luaState);
  int addButtonFlipChance(lua_State* luaState);
  int addButtonPressChance(lua_State* luaState);
  int addButtonReleaseChance(lua_State* luaState);
  int addOrSubtractFromCurrentAnalogValueChance(lua_State* luaState);
  int addOrSubtractFromSpecificAnalogValueChance(lua_State* luaState);
  int addButtonComboChance(lua_State* luaState);
  int addControllerClearChance(lua_State* luaState);
  int getControllerInputs(lua_State* luaState);
  }
