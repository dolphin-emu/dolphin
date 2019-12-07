#pragma once

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime
{

class MP2 : public PrimeMod
{
public:
  Game game() const override { return Game::PRIME_2; }

  void beam_change_code(u32 base_offset);

  void run_mod();

  virtual ~MP2() {}

protected:
  virtual uint32_t load_state_address() const = 0;
  // Honestly unsure of what this was, found it so long ago
  // seems to point to a lot of important stuff though
  virtual uint32_t camera_ctl_address() const = 0;
  virtual uint32_t lockon_address() const = 0;
  virtual uint32_t new_beam_address() const = 0;
  virtual uint32_t beamchange_flag_address() const = 0;
  virtual uint32_t camera_ptr_address() const = 0;
  virtual uint32_t camera_offset_address() const = 0;

  //void beam_change_code(uint32_t base_offset);

private:
  float pitch = 0;
};

class MP2NTSC : public MP2
{
public:
  MP2NTSC();

  Region region() const override { return Region::NTSC; }

  virtual ~MP2NTSC() {}

protected:
  uint32_t load_state_address() const override;
  uint32_t camera_ctl_address() const override;
  uint32_t lockon_address() const override;
  uint32_t new_beam_address() const override;
  uint32_t beamchange_flag_address() const override;
  uint32_t camera_ptr_address() const override;
  uint32_t camera_offset_address() const override;
};

class MP2PAL : public MP2
{
public:
  MP2PAL();

  Region region() const override { return Region::PAL; }

  virtual ~MP2PAL() {}

protected:
  uint32_t load_state_address() const override;
  uint32_t camera_ctl_address() const override;
  uint32_t lockon_address() const override;
  uint32_t new_beam_address() const override;
  uint32_t beamchange_flag_address() const override;
  uint32_t camera_ptr_address() const override;
  uint32_t camera_offset_address() const override;
};

}
