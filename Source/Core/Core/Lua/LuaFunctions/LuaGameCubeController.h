#pragma once

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

#include "Core/HW/GCPad.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"
#include "Core/Movie.h"
#include "../LuaHelperClasses/LuaGameCubeButtonProbabilityClasses.h"
#include "../LuaHelperClasses/LuaColonCheck.h"
#include <Array>
#include <memory>
#include <vector>

namespace Lua
{
namespace LuaGameCubeController
{

  extern std::array<bool, 4> overwriteControllerAtSpecifiedPort;
  extern std::array<bool, 4> addToControllerAtSpecifiedPort;
  extern std::array<bool, 4> doRandomInputEventsAtSpecifiedPort;


  extern std::array<Movie::ControllerState, 4> newOverwriteControllerInputs;
  extern std::array<Movie::ControllerState, 4> addToControllerInputs;
  extern std::array<std::vector<GC_BUTTON_NAME>, 4> buttonListsForAddToControllerInputs;
  extern std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4> randomButtonEvents;
  
  void InitLuaGameCubeControllerFunctions(lua_State* luaState);

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
}
