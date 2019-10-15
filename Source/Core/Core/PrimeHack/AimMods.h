#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

  class MP1 : public PrimeMod {
  public:
    Game game() const override {
      return Game::PRIME_1;
    }
    void run_mod() override;

    virtual ~MP1() {}

  protected:
    virtual uint32_t lockon_address() const = 0;
    virtual uint32_t yaw_vel_address() const = 0;
    virtual uint32_t pitch_address() const = 0;
    virtual uint32_t pitch_goal_address() const = 0;
    virtual uint32_t avel_limiter_address() const = 0;
    virtual uint32_t new_beam_address() const = 0;
    virtual uint32_t beamchange_flag_address() const = 0;
    virtual uint32_t visor_base_address() const = 0;
    virtual uint32_t camera_pointer_address() const = 0;
    virtual uint32_t active_camera_offset_address() const = 0;
    virtual uint32_t global_fov1() const = 0;
    virtual uint32_t global_fov2() const = 0;

    void beam_change_code(uint32_t base_offset);

  private:
    float pitch = 0;
  };

  class MP1NTSC : public MP1 {
  public:
    MP1NTSC();

    Region region() const override {
      return Region::NTSC;
    }

    virtual ~MP1NTSC() {}

  protected:
    uint32_t lockon_address() const override;
    uint32_t yaw_vel_address() const override;
    uint32_t pitch_address() const override;
    uint32_t pitch_goal_address() const override;
    uint32_t avel_limiter_address() const override;
    uint32_t new_beam_address() const override;
    uint32_t beamchange_flag_address() const override;
    uint32_t visor_base_address() const override;
    uint32_t camera_pointer_address() const override;
    uint32_t active_camera_offset_address() const override;
    uint32_t global_fov1() const override;
    uint32_t global_fov2() const override;
  };

  class MP1PAL : public MP1 {
  public:
    MP1PAL();

    Region region() const override {
      return Region::PAL;
    }

    virtual ~MP1PAL() {}

  protected:
    uint32_t lockon_address() const override;
    uint32_t yaw_vel_address() const override;
    uint32_t pitch_address() const override;
    uint32_t pitch_goal_address() const override;
    uint32_t avel_limiter_address() const override;
    uint32_t new_beam_address() const override;
    uint32_t beamchange_flag_address() const override;
    uint32_t visor_base_address() const override;
    uint32_t camera_pointer_address() const override;
    uint32_t active_camera_offset_address() const override;
    uint32_t global_fov1() const override;
    uint32_t global_fov2() const override;
  };

  class MP2 : public PrimeMod {
  public:
    Game game() const override {
      return Game::PRIME_2;
    }

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

    void beam_change_code(uint32_t base_offset);

  private:
    float pitch = 0;
  };

  class MP2NTSC : public MP2 {
  public:
    MP2NTSC();

    Region region() const override {
      return Region::NTSC;
    }

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

  class MP2PAL : public MP2 {
  public:
    MP2PAL();

    Region region() const override {
      return Region::PAL;
    }

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

  class MP3 : public PrimeMod {
  public:
    Game game() const override {
      return Game::PRIME_3;
    }

    void run_mod() override;

    bool should_apply_changes() const {
      return !fighting_ridley && PrimeMod::should_apply_changes();
    }

    virtual ~MP3() {}

  protected:
    virtual uint32_t camera_ctl_address() const = 0;
    virtual uint32_t grapple_hook_address() const = 0;
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

  private:
    float pitch = 0;
    bool fighting_ridley = false;
  };

  class MP3NTSC : public MP3 {
  public:
    MP3NTSC();

    Region region() const override {
      return Region::NTSC;
    }

    virtual ~MP3NTSC() {}

  protected:
    uint32_t camera_ctl_address() const override;
    uint32_t grapple_hook_address() const override;
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

  class MP3PAL : public MP3 {
  public:
    MP3PAL();

    Region region() const override {
      return Region::PAL;
    }

    virtual ~MP3PAL() {}

  protected:
    uint32_t camera_ctl_address() const override;
    uint32_t grapple_hook_address() const override;
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

  class MenuNTSC : public PrimeMod {
  public:
    Game game() const override {
      return Game::MENU;
    }

    Region region() const override {
      return Region::NTSC;
    }

    void run_mod() override;

    virtual ~MenuNTSC() {}
  };

  class MenuPAL : public PrimeMod {
  public:
    Game game() const override {
      return Game::MENU;
    }

    Region region() const override {
      return Region::PAL;
    }

    void run_mod() override;

    virtual ~MenuPAL() {}
  };
}
