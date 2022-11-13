#include "LuaEmuFunctions.h"
#include "Core/Core.h"

namespace Lua
{
namespace Lua_emu
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

  luaL_Reg luaEmuFunctions[] = {{"frameAdvance", emu_frameAdvance}, {nullptr, nullptr}};

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

}  // namespace Lua_emu
}  // namespace Lua
