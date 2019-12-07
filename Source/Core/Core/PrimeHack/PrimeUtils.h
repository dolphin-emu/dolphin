#pragma once

#include "Core/PrimeHack/PrimeMod.h"

#include <cmath>
#include <sstream>

#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/HackConfig.h"
#include "InputCommon/GenericMouse.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"

extern std::string cplayer_str;

namespace prime
{
constexpr float TURNRATE_RATIO = 0.00498665500569808449206349206349f;

int get_beam_switch(std::array<int, 4> const& beams);
std::tuple<int, int> get_visor_switch(std::array<std::tuple<int, int>, 4> const& visors, bool combat_visor);

void handle_cursor(u32 x_address, u32 y_address, float right_bound, float bottom_bound);

void springball_code(u32 base_offset, std::vector<CodeChange>* code_changes);
void springball_check(u32 ball_address, u32 movement_address);

void set_cplayer_str(u32 address);

bool mem_check(u32 address);
void write_invalidate(u32 address, u32 value);
float getAspectRatio();

void set_beam_owned(int index, bool owned);
void set_cursor_pos(float x, float y);

class MenuNTSC : public PrimeMod
{
public:
  Game game() const override { return Game::MENU; }

  Region region() const override { return Region::NTSC; }

  void run_mod() override;

  virtual ~MenuNTSC() {}
};

class MenuPAL : public PrimeMod
{
public:
  Game game() const override { return Game::MENU; }

  Region region() const override { return Region::PAL; }

  void run_mod() override;

  virtual ~MenuPAL() {}
};
}  // namespace prime
