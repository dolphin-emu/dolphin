#pragma once

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime
{

class MP3 : public PrimeMod
{
public:
  Game game() const override { return Game::PRIME_3; }

  void run_mod() override;

  bool should_apply_changes() const { return !fighting_ridley && PrimeMod::should_apply_changes(); }

  virtual ~MP3() {}

protected:
  virtual uint32_t camera_ctl_address() const = 0;
  virtual uint32_t cursor_enabled_address() const = 0;
  virtual uint32_t boss_id_address() const = 0;
  virtual uint32_t cursor_dlg_address() const = 0;
  virtual uint32_t cursor_address() const = 0;
  virtual uint32_t cursor_offset() const = 0;
  virtual uint32_t cannon_lag_rtoc_offset() const = 0;
  virtual uint32_t lockon_address() const = 0;
  virtual uint32_t camera_pointer_address() const = 0;
  virtual void apply_mod_instructions() = 0;
  virtual void apply_normal_instructions() = 0;

  void control_state_hook(u32 base_offset, Region region);
  void grapple_slide_no_lookat(u32 base_offset);

private:
  float pitch = 0;
  bool fighting_ridley = false;
};

class MP3NTSC : public MP3
{
public:
  MP3NTSC();

  Region region() const override { return Region::NTSC; }

  virtual ~MP3NTSC() {}

protected:
  uint32_t camera_ctl_address() const override;
  uint32_t cursor_enabled_address() const override;
  uint32_t boss_id_address() const override;
  uint32_t cursor_dlg_address() const;
  uint32_t cursor_address() const override;
  uint32_t cursor_offset() const override;
  uint32_t cannon_lag_rtoc_offset() const override;
  uint32_t lockon_address() const override;
  uint32_t camera_pointer_address() const override;
  void apply_mod_instructions() override;
  void apply_normal_instructions() override;
};

class MP3PAL : public MP3
{
public:
  MP3PAL();

  Region region() const override { return Region::PAL; }

  virtual ~MP3PAL() {}

protected:
  uint32_t camera_ctl_address() const override;
  uint32_t cursor_enabled_address() const override;
  uint32_t boss_id_address() const override;
  uint32_t cursor_dlg_address() const;
  uint32_t cursor_address() const override;
  uint32_t cursor_offset() const override;
  uint32_t cannon_lag_rtoc_offset() const override;
  uint32_t lockon_address() const override;
  uint32_t camera_pointer_address() const override;
  void apply_mod_instructions() override;
  void apply_normal_instructions() override;
};

}
