#include "LuaGameCubeButtonProbabilityClasses.h"

std::mt19937_64 LuaGameCubeButtonProbabilityEvent::numGenerator_64Bits;

void LuaGameCubeButtonProbabilityEvent::setRngSeeding()
{
  LuaGameCubeButtonProbabilityEvent::numGenerator_64Bits.seed(time(nullptr));
}
