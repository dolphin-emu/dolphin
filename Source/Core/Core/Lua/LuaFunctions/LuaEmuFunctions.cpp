#include "LuaEmuFunctions.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

namespace Lua
{
namespace LuaEmu
{
emu* emuPointer = NULL;

bool luaScriptActive = false;
std::mutex frameAdvanceLock;
std::condition_variable frameAdvanceConditionalVariable;

emu* GetEmuInstance()
{
  if (emuPointer == NULL)
    emuPointer = new emu();
  return emuPointer;
}

void InitLuaEmuFunctions(lua_State* luaState)
{
  emu** emuPtrPtr = (emu**)lua_newuserdata(luaState, sizeof(emu*));
  *emuPtrPtr = GetEmuInstance();
  luaL_newmetatable(luaState, "LuaEmuMetaTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaEmuFunctions[] = {{"frameAdvance", emu_frameAdvance}, {"getRegister", emu_get_register}, {nullptr, nullptr}};

  luaL_setfuncs(luaState, luaEmuFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "emu");
}

void StatePauseFunction()
{
  Core::SetState(Core::State::Paused);
}

int emu_frameAdvance(lua_State* luaState)
{
  //(emu**)luaL_checkudata(luaState, 1, "LuaEmuMetaTable");
  std::unique_lock<std::mutex> lk(frameAdvanceLock);
  if (Core::GetState() != Core::State::Paused)
    Core::QueueHostJob(StatePauseFunction);
  Core::QueueHostJob(Core::DoFrameStep);
  frameAdvanceConditionalVariable.wait_until(
      lk, std::chrono::time_point(std::chrono::system_clock::now() + std::chrono::hours(10000)));
  frameAdvanceConditionalVariable.notify_all();
  return 0;
}

void convertToUpperCase(std::string& inputString)
{
  for (int i = 0; i < inputString.length(); ++i)
  {
    inputString[i] = std::toupper(inputString[i]);
  }
}

//Returns the specified register (passed as a string in argument 1 ex. "PC"). All regular 32 bit registers are returned as an unsigned int.
//All floating point registers are returned as doubles.
//Supports PC, the 32 general purpose registers, and the floating point registers.
int emu_get_register(lua_State* luaState)
{
  std::string registerName = luaL_checkstring(luaState, 2);
  convertToUpperCase(registerName);
  if (registerName.length() < 2)
    luaL_error(luaState, "Error: getRegister() contained string with length less than 2");
  if (registerName[0] == 'R')
  {
    u32 registerArrayIndex = std::stoi(registerName.substr(1));
    if (registerArrayIndex > 31)
      luaL_error(luaState, (std::string("Error: register ") + registerName + " is outside the range of general purpose registers (which go from R0 to R31)").c_str());
    lua_pushinteger(luaState, PowerPC::ppcState.gpr[registerArrayIndex]);
  }
  else if (registerName == "PC")
    lua_pushinteger(luaState, PowerPC::ppcState.pc);
  else
    luaL_error(luaState, "Error: So far, only R0...R31 and PC are supported in getRegister()");
  return 1;
}

}  // namespace Lua_emu
}  // namespace Lua
