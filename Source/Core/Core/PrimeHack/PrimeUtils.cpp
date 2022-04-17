#include "Core/PrimeHack/PrimeUtils.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/PowerPC/MMU.h"
#include "VideoCommon/VideoConfig.h"

#include "Common/Timer.h"
#include "Common/BitUtils.h"

#include <cstdarg>

std::string info_str;

namespace prime
{
/*
* Prime one beam IDs: 0 = power, 1 = ice, 2 = wave, 3 = plasma
* Prime one visor IDs: 0 = combat, 1 = xray, 2 = scan, 3 = thermal
* Prime two beam IDs: 0 = power, 1 = dark, 2 = light, 3 = annihilator
* Prime two visor IDs: 0 = combat, 1 = echo, 2 = scan, 3 = dark
* ADDITIONAL INFO: Equipment have-status offsets:
* Beams can be ignored (for now) as the existing code handles that for us
* Prime one visor offsets: combat = 0x11, scan = 0x05, thermal = 0x09, xray = 0x0d
* Prime two visor offsets: combat = 0x08, scan = 0x09, dark = 0x0a, echo = 0x0b
*/

static float cursor_x = 0, cursor_y = 0;
static int current_beam = 0;
static int current_visor = 0;
static std::array<bool, 4> beam_owned = {false, false, false, false};
static std::array<bool, 4> visor_owned = {false, false, false, false};
static bool noclip_enabled = false;
static bool was_in_alternate = false;

std::array<std::array<CodeChange, static_cast<int>(Game::MAX_VAL) + 1>,
  static_cast<int>(Region::MAX_VAL) + 1> noclip_enable_codes;
std::array<std::array<CodeChange, static_cast<int>(Game::MAX_VAL) + 1>,
  static_cast<int>(Region::MAX_VAL) + 1> noclip_disable_codes;

static u32 noclip_msg_time;
static u32 invulnerability_msg_time;
static u32 cutscene_msg_time;
static u32 scandash_msg_time;

u8 read8(u32 addr) {
  return PowerPC::HostRead_U8(addr);
}

u16 read16(u32 addr) {
  return PowerPC::HostRead_U16(addr);
}

u32 read32(u32 addr) {
  return PowerPC::HostRead_U32(addr);
}

u32 readi(u32 addr) {
  return PowerPC::HostRead_Instruction(addr);
}

u64 read64(u32 addr) {
  return PowerPC::HostRead_U64(addr);
}

float readf32(u32 addr) {
  return Common::BitCast<float>(read32(addr));
}

void write8(u8 var, u32 addr) {
  PowerPC::HostWrite_U8(var, addr);
}

void write16(u16 var, u32 addr) {
  PowerPC::HostWrite_U16(var, addr);
}

void write32(u32 var, u32 addr) {
  PowerPC::HostWrite_U32(var, addr);
}

void write64(u64 var, u32 addr) {
  PowerPC::HostWrite_U64(var, addr);
}

void writef32(float var, u32 addr) {
  PowerPC::HostWrite_F32(var, addr);
}

void set_beam_owned(int index, bool owned) {
  beam_owned[index] = owned;
}

void set_visor_owned(int index, bool owned) {
  visor_owned[index] = owned;
}

void set_cursor_pos(float x, float y) {
  cursor_x = x;
  cursor_y = y;
}

void write_invalidate(u32 address, u32 value) {
  write32(value, address);
  PowerPC::ScheduleInvalidateCacheThreadSafe(address);
}

std::tuple<int, int> get_visor_switch(std::array<std::tuple<int, int>, 4> const& visors, bool combat_visor) {
  static bool pressing_button = false;
  if (CheckVisorCtl(0)) {
    if (!combat_visor) {
      if (!pressing_button) {
        pressing_button = true;
        return visors[0];
      }
      else {
        return std::make_tuple(-1, 0);
      }
    }
  }
  if (CheckVisorCtl(1)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[1];
    }
  }
  else if (CheckVisorCtl(2)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[2];
    }
  }
  else if (CheckVisorCtl(3)) {
    if (!pressing_button) {
      pressing_button = true;
      return visors[3];
    }
  }
  else if (CheckVisorScrollCtl(true)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (visor_owned[current_visor = (current_visor + 1) % 4]) break;
      }
      return visors[current_visor];
    }
  }
  else if (CheckVisorScrollCtl(false)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (visor_owned[current_visor = (current_visor + 3) % 4]) break;
      }
      return visors[current_visor];
    }
  }
  else {
    pressing_button = false;
  }
  return std::make_tuple(-1, 0);
}

int get_beam_switch(std::array<int, 4> const& beams) {
  static bool pressing_button = false;
  if (CheckBeamCtl(0)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[0];
    }
  }
  else if (CheckBeamCtl(1)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[1];
    }
  }
  else if (CheckBeamCtl(2)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[2];
    }
  }
  else if (CheckBeamCtl(3)) {
    if (!pressing_button) {
      pressing_button = true;
      return current_beam = beams[3];
    }
  }
  else if (CheckBeamScrollCtl(true)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (beam_owned[current_beam = (current_beam + 1) % 4]) break;
      }
      return beams[current_beam];
    }
  }
  else if (CheckBeamScrollCtl(false)) {
    if (!pressing_button) {
      pressing_button = true;
      for (int i = 0; i < 4; i++) {
        if (beam_owned[current_beam = (current_beam + 3) % 4]) break;
      }
      return beams[current_beam];
    }
  }
  else {
    pressing_button = false;
  }
  return -1;
}

void swap_alt_profiles(u32 ball_state, u32 transition_state)
{
  /* Ball State 1 - Morphed, Transition State 1 - Map */
  if ((ball_state == 1 || ball_state == 2 || transition_state == 1) && !was_in_alternate)
  {
    std::string profile = GetProfiles().first;

    if (!profile.empty() && (profile != std::string("Disabled"))) {
      ChangeControllerProfileAlt(profile);
    }
    was_in_alternate = true;
  }
  else if ((ball_state == 0 && transition_state != 1) && was_in_alternate)
  {
    std::string profile = GetProfiles().second;

    ChangeControllerProfileAlt(profile);
    was_in_alternate = false;
  }
}

std::stringstream ss;
void DevInfo(const char* name, const char* format, ...)
{
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);

  std::vector<char> buf(1+std::vsnprintf(nullptr, 0, format, args1));
  std::vsnprintf(buf.data(), buf.size(), format, args2);

  ss << name << ": " << buf.data() << std::endl;
  va_end(args2);
}

void DevInfoMatrix(const char* name, const Transform& t)
{
  const char* format =
    "\n   %.3f    %.3f    %.3f    %.3f"
    "\n   %.3f    %.3f    %.3f    %.3f"
    "\n   %.3f    %.3f    %.3f    %.3f";

  u32 bufsize = snprintf(NULL, 0, format, 
    t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3], 
    t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3], 
    t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);

  std::vector<char> buf;
  buf.resize(bufsize + 1);

  snprintf(buf.data(), bufsize + 1, format,
    t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3], 
    t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3], 
    t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);

  ss << name << ":" << buf.data() << std::endl;
}

std::string GetDevInfo()
{
  std::string result = ss.str();

  return result;
}

void ClrDevInfo()
{
  ss = std::stringstream();
}

bool mem_check(u32 address) {
  return (address >= 0x80000000) && (address < 0x81800000);
}

constexpr float kMetroidAr = 16.f / 9.f;
constexpr float kInvMetroidAr = 1.f / kMetroidAr;

// Why do I need this? To fudge the numbers till it works
constexpr float kPalStretchMultiplier = 1.021875f;
constexpr float kNtscStretchMultiplier = 1.003125f;

constexpr float kNtscHalfHRange = 1.025f;
constexpr float kNtscHalfVRange_16_9 = 0.715f;
constexpr float kNtscHalfVRange_16_10 = 0.8f;
constexpr float kNtscHalfVRange_4_3 = 0.95f;
constexpr float kPalHalfHRange = 0.87f;
constexpr float kPalHalfVRange_16_9 = 0.73f;
constexpr float kPalHalfVRange_16_10 = 0.815f;
constexpr float kPalHalfVRange_4_3 = 0.965f;

constexpr float kMenuHalfVRange = 1.f;

static AspectMode get_aspect_mode() {
  AspectMode mode = Config::Get(Config::GFX_ASPECT_RATIO);
  if (mode != AspectMode::Stretch) {
    return mode;
  }
  return Config::Get(Config::GFX_WIDESCREEN_HACK) ? AspectMode::Stretch : AspectMode::AnalogWide;
}

static void handle_wiimote_IR(u32 x_address, u32 y_address, Region region, float half_width, float half_height, AspectMode mode, float aspect_step) {
  constexpr float kInternalWidth = 640.f;
  const float cur_width = g_renderer->GetBackbufferWidth();
  const float cur_height = g_renderer->GetBackbufferHeight(); 
  float unstretched_w = kMetroidAr > (cur_width / cur_height) ? cur_width : cur_height * kMetroidAr;
  if (region == Region::PAL) {
    unstretched_w *= kPalStretchMultiplier;
  } else {
    unstretched_w *= kNtscStretchMultiplier;
  }

  // Special casing for widescreen hack + stretch to fit
  // Not perfect, but does the job
  if (mode == AspectMode::Stretch) {
    float stretched_halfw = std::max((cur_width * half_width) / unstretched_w, half_width);
    float stretched_halfh;
    // If stretching is occurring upwards
    if (kMetroidAr > (cur_width / cur_height)) {
      const float unstretched_h = cur_width * kInvMetroidAr;
      stretched_halfh = cur_height * half_height / unstretched_h;
    } else {
      stretched_halfh = half_height;
    }
    half_width = stretched_halfw;
    half_height = stretched_halfh;
  }

  int dx = GetHorizontalAxis(), dy = GetVerticalAxis();
  float cursor_sensitivity_conv = prime::GetCursorSensitivity() / 10000.f;
  // Ironically name is backwards, the sensitivity technically scales up as the window gets larger
  // since we send normalized coordinates
  if (!prime::ScaleCursorSensitivity()) {
    cursor_sensitivity_conv *= (kInternalWidth / unstretched_w);
  }

  cursor_x += static_cast<float>(dx) * cursor_sensitivity_conv;
  cursor_y += static_cast<float>(dy) * cursor_sensitivity_conv * aspect_step;

  cursor_x = std::clamp(cursor_x, -half_width, half_width);
  cursor_y = std::clamp(cursor_y, -half_height, half_height);

  writef32(cursor_x, x_address);
  writef32(cursor_y, y_address);
}

void handle_cursor(u32 x_address, u32 y_address, Region region) {
  const AspectMode aspect_mode = get_aspect_mode();
  if (aspect_mode == AspectMode::Stretch) {
    const float cur_width = g_renderer->GetBackbufferWidth();
    const float cur_height = g_renderer->GetBackbufferHeight();
    const float render_aspect = cur_width / cur_height;
    handle_wiimote_IR(x_address, y_address, region, (region == Region::PAL ? kPalHalfHRange : kNtscHalfHRange), kMenuHalfVRange, AspectMode::AnalogWide, render_aspect);
  } else {
    handle_wiimote_IR(x_address, y_address, region, (region == Region::PAL ? kPalHalfHRange : kNtscHalfHRange), kMenuHalfVRange, aspect_mode, kMetroidAr);
  }
}

void handle_reticle(u32 x_address, u32 y_address, Region region, float fov) {
  constexpr float kDegToRad = 3.141592654f / 180.f;
  constexpr float kTan30 = 0.57735026918962576450914878050196f;

  const float fov_scaling = tan((fov / 2.f) * kDegToRad) / kTan30;
  const AspectMode aspect_mode = get_aspect_mode();
  const float base_cursor_range_w = (region == Region::PAL ? kPalHalfHRange : kNtscHalfHRange) * fov_scaling;
  float base_cursor_range_h;
  switch (aspect_mode) {
  default:
  case AspectMode::Auto:
  case AspectMode::Stretch:
  case AspectMode::AnalogWide:
    base_cursor_range_h = (region == Region::PAL ? kPalHalfVRange_16_9 : kNtscHalfVRange_16_9) * fov_scaling;
    break;
  case AspectMode::Analog:
    base_cursor_range_h = (region == Region::PAL ? kPalHalfVRange_4_3 : kNtscHalfVRange_4_3) * fov_scaling;
    break;
  }

  handle_wiimote_IR(x_address, y_address, region, base_cursor_range_w, base_cursor_range_h, aspect_mode, kMetroidAr);
}
}  // namespace prime
