#pragma once

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime
{

class MP1 : public PrimeMod
{
public:
  Game game() const override { return Game::PRIME_1; }

  void run_mod() override;

  virtual ~MP1() {}

protected:
  virtual uint32_t orbit_state_address() const = 0;
  virtual uint32_t lockon_address() const = 0;
  virtual uint32_t yaw_vel_address() const = 0;
  virtual uint32_t pitch_address() const = 0;
  virtual uint32_t pitch_goal_address() const = 0;
  virtual uint32_t avel_limiter_address() const = 0;
  virtual uint32_t new_beam_address() const = 0;
  virtual uint32_t beamchange_flag_address() const = 0;
  virtual uint32_t powerups_base_address() const = 0;
  virtual uint32_t camera_pointer_address() const = 0;
  virtual uint32_t cplayer() const = 0;
  virtual uint32_t active_camera_offset_address() const = 0;
  virtual uint32_t global_fov1() const = 0;
  virtual uint32_t global_fov2() const = 0;

  void beam_change_code(uint32_t base_offset);

private:
  float pitch = 0;
};

class MP1NTSC : public MP1
{
public:
  MP1NTSC();

  Region region() const override { return Region::NTSC; }

  virtual ~MP1NTSC() {}

protected:
  uint32_t orbit_state_address() const override;
  uint32_t lockon_address() const override;
  uint32_t yaw_vel_address() const override;
  uint32_t pitch_address() const override;
  uint32_t pitch_goal_address() const override;
  uint32_t avel_limiter_address() const override;
  uint32_t new_beam_address() const override;
  uint32_t beamchange_flag_address() const override;
  uint32_t powerups_base_address() const override;
  uint32_t camera_pointer_address() const override;
  uint32_t cplayer() const;
  uint32_t active_camera_offset_address() const override;
  uint32_t global_fov1() const override;
  uint32_t global_fov2() const override;
};

class MP1PAL : public MP1
{
public:
  MP1PAL();

  Region region() const override { return Region::PAL; }

  virtual ~MP1PAL() {}

protected:
  uint32_t orbit_state_address() const override;
  uint32_t lockon_address() const override;
  uint32_t yaw_vel_address() const override;
  uint32_t pitch_address() const override;
  uint32_t pitch_goal_address() const override;
  uint32_t avel_limiter_address() const override;
  uint32_t new_beam_address() const override;
  uint32_t beamchange_flag_address() const override;
  uint32_t powerups_base_address() const override;
  uint32_t camera_pointer_address() const override;
  uint32_t cplayer() const;
  uint32_t active_camera_offset_address() const override;
  uint32_t global_fov1() const override;
  uint32_t global_fov2() const override;
};

}
