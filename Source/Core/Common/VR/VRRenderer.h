// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/VRDisplay.h"

namespace Common::VR
{
enum ConfigFloat
{
  // 2D canvas positioning
  CONFIG_CANVAS_DISTANCE,
  CONFIG_MENU_PITCH,
  CONFIG_MENU_YAW,
  CONFIG_RECENTER_YAW,
  CONFIG_CANVAS_ASPECT,

  CONFIG_FLOAT_MAX
};

enum ConfigInt
{
  // switching between 2D and 3D
  CONFIG_MODE,
  // mouse cursor
  CONFIG_MOUSE_SIZE,
  CONFIG_MOUSE_X,
  CONFIG_MOUSE_Y,
  // viewport setup
  CONFIG_VIEWPORT_WIDTH,
  CONFIG_VIEWPORT_HEIGHT,
  CONFIG_VIEWPORT_VALID,
  // render status
  CONFIG_CURRENT_FBO,

  // end
  CONFIG_INT_MAX
};

enum RenderMode
{
  RENDER_MODE_MONO_SCREEN,
  RENDER_MODE_STEREO_SCREEN,
  RENDER_MODE_MONO_6DOF,
  RENDER_MODE_STEREO_6DOF
};

class Renderer
{
public:
  void GetResolution(engine_t* engine, int* pWidth, int* pHeight);
  void Init(engine_t* engine, bool multiview);
  void Destroy(engine_t* engine);

  bool InitFrame(engine_t* engine);
  void BeginFrame(engine_t* engine, int fboIndex);
  void EndFrame(engine_t* engine);
  void FinishFrame(engine_t* engine);

  float GetConfigFloat(ConfigFloat config);
  void SetConfigFloat(ConfigFloat config, float value);
  int GetConfigInt(ConfigInt config);
  void SetConfigInt(ConfigInt config, int value);

  void BindFramebuffer(engine_t* engine);
  XrView GetView(int eye);
  XrVector3f GetHMDAngles();

private:
  void Recenter(engine_t* engine);
  void UpdateStageBounds(engine_t* engine);

private:
  bool m_initialized = false;
  bool m_stage_supported = false;
  float m_config_float[CONFIG_FLOAT_MAX] = {};
  int m_config_int[CONFIG_INT_MAX] = {};

  XrFovf m_fov;
  XrView* m_projections;
  XrPosef m_inverted_view_pose[2];
  XrVector3f m_hmd_orientation;
  XrFrameState m_frame_state = {};
};
}  // namespace Common::VR
