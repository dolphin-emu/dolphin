#include "LuaGameCubeButtonProbabilityClasses.h"

std::mt19937_64 LuaGameCubeButtonProbabilityEvent::num_generator_64_bits;

void LuaGameCubeButtonProbabilityEvent::SetRngSeeding()
{
  LuaGameCubeButtonProbabilityEvent::num_generator_64_bits.seed(time(nullptr));
}
