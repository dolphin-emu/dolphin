// Copyright 2016 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VRGameMatrices.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D/AvatarDrawer.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/tiny_obj_loader.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"

extern float s_fViewTranslationVector[3];
extern EFBRectangle g_final_screen_region;

bool CalculateViewMatrix(int kind, Matrix44& look_matrix)
{
  bool bStuckToHead = false, bIsSkybox = false, bIsPerspective = false,
       bHasWidest = (vr_widest_3d_HFOV > 0);
  bool bIsHudElement = false, bIsOffscreen = false, bAspectWide = true, bNoForward = false,
       bShowAim = false;
  float UnitsPerMetre = 1.0f;

  // Show HUD
  if (kind == 1)
  {
    if (!bHasWidest)
      return false;
  }
  // Show Aim
  else if (kind == 0)
  {
    if (bHasWidest)
      bShowAim = true;
  }
  // Show 2D
  else if (kind == 2)
  {
    bHasWidest = false;
  }

  float zoom_forward = 0.0f;
  if (vr_widest_3d_HFOV <= g_ActiveConfig.fMinFOV && bHasWidest)
  {
    zoom_forward = g_ActiveConfig.fAimDistance *
                   tanf(DEGREES_TO_RADIANS(g_ActiveConfig.fMinFOV) / 2) /
                   tanf(DEGREES_TO_RADIANS(vr_widest_3d_HFOV) / 2);
    zoom_forward -= g_ActiveConfig.fAimDistance;
  }

  float hfov = 0, vfov = 0;
  hfov = vr_widest_3d_HFOV;
  vfov = vr_widest_3d_VFOV;

  // VR Headtracking and leaning back compensation
  Matrix44 rotation_matrix;
  Matrix44 lean_back_matrix;
  Matrix44 camera_pitch_matrix;
  if (bStuckToHead)
  {
    Matrix44::LoadIdentity(rotation_matrix);
    Matrix44::LoadIdentity(lean_back_matrix);
    Matrix44::LoadIdentity(camera_pitch_matrix);
  }
  else
  {
    // head tracking
    if (g_ActiveConfig.bOrientationTracking)
    {
      VR_UpdateHeadTrackingIfNeeded();
      rotation_matrix = g_head_tracking_matrix;
    }
    else
    {
      Matrix44::LoadIdentity(rotation_matrix);
    }

    Matrix33 pitch_matrix33;

    // leaning back
    float extra_pitch = -g_ActiveConfig.fLeanBackAngle;
    Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
    lean_back_matrix = pitch_matrix33;

    // camera pitch
    if ((g_ActiveConfig.bStabilizePitch || g_ActiveConfig.bStabilizeRoll ||
         g_ActiveConfig.bStabilizeYaw) &&
        g_ActiveConfig.bCanReadCameraAngles &&
        (g_ActiveConfig.iMotionSicknessSkybox != 2 || !bIsSkybox))
    {
      if (!g_ActiveConfig.bStabilizePitch)
      {
        Matrix44 user_pitch44;
        Matrix44 roll_and_yaw_matrix;

        if (bIsPerspective || bHasWidest)
          extra_pitch = g_ActiveConfig.fCameraPitch;
        else
          extra_pitch = g_ActiveConfig.fScreenPitch;
        Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
        user_pitch44 = pitch_matrix33;
        roll_and_yaw_matrix = g_game_camera_rotmat;
        camera_pitch_matrix = user_pitch44 * roll_and_yaw_matrix;
      }
      else
      {
        camera_pitch_matrix = g_game_camera_rotmat;
      }
    }
    else
    {
      if (xfmem.projection.type == GX_PERSPECTIVE || bHasWidest)
        extra_pitch = g_ActiveConfig.fCameraPitch;
      else
        extra_pitch = g_ActiveConfig.fScreenPitch;
      Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
      camera_pitch_matrix = pitch_matrix33;
    }
  }

  // Position matrices
  Matrix44 head_position_matrix, free_look_matrix, camera_forward_matrix, camera_position_matrix;
  if (bStuckToHead || bIsSkybox)
  {
    Matrix44::LoadIdentity(head_position_matrix);
    Matrix44::LoadIdentity(free_look_matrix);
    Matrix44::LoadIdentity(camera_position_matrix);
  }
  else
  {
    float pos[3];
    // head tracking
    if (g_ActiveConfig.bPositionTracking)
    {
      for (int i = 0; i < 3; ++i)
        pos[i] = g_head_tracking_position[i] * UnitsPerMetre;
      Matrix44::Translate(head_position_matrix, pos);
    }
    else
    {
      Matrix44::LoadIdentity(head_position_matrix);
    }

    // freelook camera position
    for (int i = 0; i < 3; ++i)
      pos[i] = s_fViewTranslationVector[i] * UnitsPerMetre;
    Matrix44::Translate(free_look_matrix, pos);

    // camera position stabilisation
    if (g_ActiveConfig.bStabilizeX || g_ActiveConfig.bStabilizeY || g_ActiveConfig.bStabilizeZ)
    {
      for (int i = 0; i < 3; ++i)
        pos[i] = -g_game_camera_pos[i] * UnitsPerMetre;
      Matrix44::Translate(camera_position_matrix, pos);
    }
    else
    {
      Matrix44::LoadIdentity(camera_position_matrix);
    }
  }

  if (bIsPerspective && !bIsHudElement && !bIsOffscreen)
  {
    // Transformations must be applied in the following order for VR:
    // camera position stabilisation
    // camera forward
    // camera pitch
    // free look
    // leaning back
    // head position tracking
    // head rotation tracking
    if (bNoForward || bIsSkybox || bStuckToHead)
    {
      Matrix44::LoadIdentity(camera_forward_matrix);
    }
    else
    {
      float pos[3];
      pos[0] = 0;
      pos[1] = 0;
      pos[2] = (g_ActiveConfig.fCameraForward + zoom_forward) * UnitsPerMetre;
      Matrix44::Translate(camera_forward_matrix, pos);
    }

    look_matrix = camera_forward_matrix * camera_position_matrix * camera_pitch_matrix *
                  free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
  }
  else
  {
    float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

    // 2D Screen
    if (!bHasWidest)
    {
      HudThickness = g_ActiveConfig.fScreenThickness * UnitsPerMetre;
      HudDistance = g_ActiveConfig.fScreenDistance * UnitsPerMetre;
      HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
      HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
      // NES games are supposed to be 1.175:1 (16:13.62) even though VC normally renders them as
      // 16:9
      // http://forums.nesdev.com/viewtopic.php?t=8063
      if (g_is_nes)
        HudWidth = HudHeight * 1.175f;
      else if (bAspectWide)
        HudWidth = HudHeight * (float)16 / 9;
      else
        HudWidth = HudHeight * (float)4 / 3;
      CameraForward = 0;
      HudUp = g_ActiveConfig.fScreenUp * UnitsPerMetre;
      AimDistance = HudDistance;
    }
    else
    // HUD over 3D world
    {
      // The HUD distance might have been carefully chosen to line up with objects, so we should
      // scale it with the world
      // But we can't make the HUD too close or it's hard to look at, and we should't make the HUD
      // too far or it stops looking 3D
      const float MinHudDistance = 0.28f, MaxHudDistance = 3.00f;  // HUD shouldn't go closer than
                                                                   // 28 cm when shrinking scale, or
                                                                   // further than 3m when growing
      float HUDScale = g_ActiveConfig.fScale;
      if (HUDScale < 1.0f && g_ActiveConfig.fHudDistance >= MinHudDistance &&
          g_ActiveConfig.fHudDistance * HUDScale < MinHudDistance)
        HUDScale = MinHudDistance / g_ActiveConfig.fHudDistance;
      else if (HUDScale > 1.0f && g_ActiveConfig.fHudDistance <= MaxHudDistance &&
               g_ActiveConfig.fHudDistance * HUDScale > MaxHudDistance)
        HUDScale = MaxHudDistance / g_ActiveConfig.fHudDistance;

      // Give the 2D layer a 3D effect if different parts of the 2D layer are rendered at different
      // z coordinates
      HudThickness = g_ActiveConfig.fHudThickness * HUDScale *
                     UnitsPerMetre;  // the 2D layer is actually a 3D box this many game units thick
      HudDistance = g_ActiveConfig.fHudDistance * HUDScale *
                    UnitsPerMetre;  // depth 0 on the HUD should be this far away

      HudUp = 0;
      if (bNoForward)
        CameraForward = 0;
      else
        CameraForward =
            (g_ActiveConfig.fCameraForward + zoom_forward) * g_ActiveConfig.fScale * UnitsPerMetre;
      // When moving the camera forward, correct the size of the HUD so that aiming is correct at
      // AimDistance
      AimDistance = g_ActiveConfig.fAimDistance * g_ActiveConfig.fScale * UnitsPerMetre;
      if (AimDistance <= 0)
        AimDistance = HudDistance;
      if (bShowAim)
      {
        HudThickness = 0;
        HudDistance = AimDistance;
        HUDScale = g_ActiveConfig.fScale;
      }
      // Now that we know how far away the box is, and what FOV it should fill, we can work out the
      // width and height in game units
      // Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero
      // float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
      // float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) *
      // Correction;
      HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance *
                 (AimDistance + CameraForward) / AimDistance;
      HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance *
                  (AimDistance + CameraForward) / AimDistance;
    }

    float scale[3];  // width, height, and depth of box in game units divided by 2D width, height,
                     // and depth
    float position[3];  // position of front of box relative to the camera, in game units

    float viewport_scale[2];
    float viewport_offset[2];  // offset as a fraction of the viewport's width
    if (!bIsHudElement && !bIsOffscreen)
    {
      viewport_scale[0] = 1.0f;
      viewport_scale[1] = 1.0f;
      viewport_offset[0] = 0.0f;
      viewport_offset[1] = 0.0f;
    }
    else
    {
      Viewport& v = xfmem.viewport;
      float left, top, width, height;
      left = v.xOrig - v.wd - 342;
      top = v.yOrig + v.ht - 342;
      width = 2 * v.wd;
      height = -2 * v.ht;
      float screen_width = (float)g_final_screen_region.GetWidth();
      float screen_height = (float)g_final_screen_region.GetHeight();
      viewport_scale[0] = width / screen_width;
      viewport_scale[1] = height / screen_height;
      viewport_offset[0] = ((left + (width / 2)) - (0 + (screen_width / 2))) / screen_width;
      viewport_offset[1] = -((top + (height / 2)) - (0 + (screen_height / 2))) / screen_height;
    }

    // 3D HUD elements (may be part of 2D screen or HUD)
    if (bIsPerspective)
    {
      // these are the edges of the near clipping plane in game coordinates
      float left2D = 0;
      float right2D = 1;
      float bottom2D = 1;
      float top2D = 0;
      float zFar2D = 1;
      float zNear2D = -1;
      float zObj = zNear2D + (zFar2D - zNear2D) * g_ActiveConfig.fHud3DCloser;

      left2D *= zObj;
      right2D *= zObj;
      bottom2D *= zObj;
      top2D *= zObj;

      // Scale the width and height to fit the HUD in metres
      if (right2D == left2D)
      {
        scale[0] = 0;
      }
      else
      {
        scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
      }
      if (top2D == bottom2D)
      {
        scale[1] = 0;
      }
      else
      {
        scale[1] = viewport_scale[1] * HudHeight /
                   (top2D - bottom2D);  // note that positive means up in 3D
      }
      // Keep depth the same scale as width, so it looks like a real object
      if (zFar2D == zNear2D)
      {
        scale[2] = scale[0];
      }
      else
      {
        scale[2] = scale[0];
      }
      // Adjust the position for off-axis projection matrices, and shifting the 2D screen
      position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                    viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
      position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                    HudUp;  // shift it up into the centre of the view;
      // Shift it from the old near clipping plane to the HUD distance, and shift the camera forward
      if (!bHasWidest)
        position[2] = scale[2] * zObj - HudDistance;
      else
        position[2] = scale[2] * zObj - HudDistance;  // - CameraForward;
    }
    // 2D layer, or 2D viewport (may be part of 2D screen or HUD)
    else
    {
      float left2D = 0;
      float right2D = 1;
      float bottom2D = 1;
      float top2D = 0;
      float zNear2D = -1;
      float zFar2D = 1;

      // for 2D, work out the fraction of the HUD we should fill, and multiply the scale by that
      // also work out what fraction of the height we should shift it up, and what fraction of the
      // width we should shift it left
      // only multiply by the extra scale after adjusting the position?

      if (right2D == left2D)
      {
        scale[0] = 0;
      }
      else
      {
        scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
      }
      if (top2D == bottom2D)
      {
        scale[1] = 0;
      }
      else
      {
        scale[1] = viewport_scale[1] * HudHeight /
                   (top2D - bottom2D);  // note that positive means up in 3D
      }
      if (zFar2D == zNear2D)
      {
        scale[2] = 0;  // The 2D layer was flat, so we make it flat instead of a box to avoid
                       // dividing by zero
      }
      else
      {
        scale[2] = HudThickness /
                   (zFar2D -
                    zNear2D);  // Scale 2D z values into 3D game units so it is the right thickness
      }
      position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                    viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
      position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                    HudUp;  // shift it up into the centre of the view;
      // Shift it from the zero plane to the HUD distance, and shift the camera forward
      if (!bHasWidest)
        position[2] = -HudDistance;
      else
        position[2] = -HudDistance;  // - CameraForward;
    }

    Matrix44 scale_matrix, position_matrix;
    Matrix44::Scale(scale_matrix, scale);
    Matrix44::Translate(position_matrix, position);

    // order: scale, position
    look_matrix = scale_matrix * position_matrix * camera_position_matrix * camera_pitch_matrix *
                  free_look_matrix * lean_back_matrix * head_position_matrix * rotation_matrix;
  }
  return true;
}

void VRCalculateIRPointer()
{
  float wmpos[3], thumb[3];
  Matrix33 wmrot;
  ControllerStyle cs = CS_HYDRA_RIGHT;
  bool has_right_controller = VR_GetRightControllerPos(wmpos, thumb, &wmrot);
  if (has_right_controller)
    cs = VR_GetHydraStyle(1);
  if (cs != CS_WIIMOTE_IR)
  {
    g_vr_has_ir = false;
    return;
  }

  Matrix44 ToAimSpace, WiimoteRot;
  CalculateTrackingSpaceToViewSpaceMatrix(0, ToAimSpace);
  WiimoteRot = wmrot;

  float r[3], rp[3], d[3], v[3] = {0, 0, -1.0f}, ppos[3], aimpoint[3];

  // find pointer position
  Matrix44::Multiply(WiimoteRot, v, ppos);
  for (int i = 0; i < 3; ++i)
    ppos[i] += wmpos[i];

  Matrix44::Multiply(ToAimSpace, wmpos, r);
  Matrix44::Multiply(ToAimSpace, ppos, rp);
  for (int i = 0; i < 3; ++i)
    d[i] = rp[i] - r[i];
  float s = -r[2] / d[2];
  if (s < 0)
  {
    // aimed away from screen, so set aim point to infinity on the closest side
    // this is for first person games where we will turn the camera towards the IR point if it is
    // off-screen
    s = std::numeric_limits<float>::infinity();
  }

  for (int i = 0; i < 3; ++i)
    aimpoint[i] = r[i] + d[i] * s;

  // NOTICE_LOG(VR, "r=%8f, %8f, %8f       %8f     d=%8f, %8f, %8f     a=%8f, %8f, %8f", r[0], r[1],
  // r[2], rp[2]-r[2], d[0], d[1], d[2], aimpoint[0], aimpoint[1], aimpoint[2]);
  g_vr_ir_x = aimpoint[0] * 2 - 1;
  g_vr_ir_y = 1 - (aimpoint[1] * 2);
  g_vr_ir_z = aimpoint[2];  // todo: currently always 0, but should be based on actual controller
                            // distance from screen, not aimpoint
  g_vr_has_ir = true;
}

bool CalculateTrackingSpaceToViewSpaceMatrix(int kind, Matrix44& look_matrix)
{
  bool bStuckToHead = false, bIsSkybox = false, bIsPerspective = false,
       bHasWidest = (vr_widest_3d_HFOV > 0);
  bool bIsHudElement = false, bIsOffscreen = false, bAspectWide = true, bNoForward = false,
       bShowAim = false;
  float UnitsPerMetre = 1.0f;

  // HUD space
  if (kind == 1)
  {
    if (!bHasWidest)
      return false;
  }
  // Aim space
  else if (kind == 0)
  {
    if (bHasWidest)
      bShowAim = true;
  }
  // 2D space
  else if (kind == 2)
  {
    bHasWidest = false;
  }

  float zoom_forward = 0.0f;
  if (vr_widest_3d_HFOV <= g_ActiveConfig.fMinFOV && bHasWidest)
  {
    zoom_forward = g_ActiveConfig.fAimDistance *
                   tanf(DEGREES_TO_RADIANS(g_ActiveConfig.fMinFOV) / 2) /
                   tanf(DEGREES_TO_RADIANS(vr_widest_3d_HFOV) / 2);
    zoom_forward -= g_ActiveConfig.fAimDistance;
  }

  float hfov = 0, vfov = 0;
  hfov = vr_widest_3d_HFOV;
  vfov = vr_widest_3d_VFOV;

  // VR ~Headtracking and~ leaning back compensation
  Matrix44 rotation_matrix;
  Matrix44 lean_back_matrix;
  Matrix44 camera_pitch_matrix;
  if (bStuckToHead)
  {
    Matrix44::LoadIdentity(rotation_matrix);
    Matrix44::LoadIdentity(lean_back_matrix);
    Matrix44::LoadIdentity(camera_pitch_matrix);
  }
  else
  {
    // no head tracking
    Matrix44::LoadIdentity(rotation_matrix);

    Matrix33 pitch_matrix33;

    // leaning back
    float extra_pitch = -g_ActiveConfig.fLeanBackAngle;
    Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
    lean_back_matrix = pitch_matrix33;

    // camera pitch
    if ((g_ActiveConfig.bStabilizePitch || g_ActiveConfig.bStabilizeRoll ||
         g_ActiveConfig.bStabilizeYaw) &&
        g_ActiveConfig.bCanReadCameraAngles &&
        (g_ActiveConfig.iMotionSicknessSkybox != 2 || !bIsSkybox))
    {
      if (!g_ActiveConfig.bStabilizePitch)
      {
        Matrix44 user_pitch44;
        Matrix44 roll_and_yaw_matrix;

        if (bIsPerspective || bHasWidest)
          extra_pitch = g_ActiveConfig.fCameraPitch;
        else
          extra_pitch = g_ActiveConfig.fScreenPitch;
        Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
        user_pitch44 = pitch_matrix33;
        roll_and_yaw_matrix = g_game_camera_rotmat;
        camera_pitch_matrix = user_pitch44 * roll_and_yaw_matrix;
      }
      else
      {
        camera_pitch_matrix = g_game_camera_rotmat;
      }
    }
    else
    {
      if (xfmem.projection.type == GX_PERSPECTIVE || bHasWidest)
        extra_pitch = g_ActiveConfig.fCameraPitch;
      else
        extra_pitch = g_ActiveConfig.fScreenPitch;
      Matrix33::RotateX(pitch_matrix33, -DEGREES_TO_RADIANS(extra_pitch));
      camera_pitch_matrix = pitch_matrix33;
    }
  }

  // Position matrices
  Matrix44 head_position_matrix, free_look_matrix, camera_forward_matrix, camera_position_matrix;
  if (bStuckToHead || bIsSkybox)
  {
    Matrix44::LoadIdentity(head_position_matrix);
    Matrix44::LoadIdentity(free_look_matrix);
    Matrix44::LoadIdentity(camera_position_matrix);
  }
  else
  {
    float pos[3];
    // no head tracking
    Matrix44::LoadIdentity(head_position_matrix);

    // freelook camera position
    for (int i = 0; i < 3; ++i)
      pos[i] = s_fViewTranslationVector[i] * UnitsPerMetre;
    Matrix44::Translate(free_look_matrix, pos);

    // camera position stabilisation
    if (g_ActiveConfig.bStabilizeX || g_ActiveConfig.bStabilizeY || g_ActiveConfig.bStabilizeZ)
    {
      for (int i = 0; i < 3; ++i)
        pos[i] = -g_game_camera_pos[i] * UnitsPerMetre;
      Matrix44::Translate(camera_position_matrix, pos);
    }
    else
    {
      Matrix44::LoadIdentity(camera_position_matrix);
    }
  }

  if (bIsPerspective && !bIsHudElement && !bIsOffscreen)
  {
    // Transformations must be applied in the following order for VR:
    // camera position stabilisation
    // camera forward
    // camera pitch
    // free look
    // leaning back
    // head position tracking
    // head rotation tracking
    if (bNoForward || bIsSkybox || bStuckToHead)
    {
      Matrix44::LoadIdentity(camera_forward_matrix);
    }
    else
    {
      float pos[3];
      pos[0] = 0;
      pos[1] = 0;
      pos[2] = (g_ActiveConfig.fCameraForward + zoom_forward) * UnitsPerMetre;
      Matrix44::Translate(camera_forward_matrix, pos);
    }

    Matrix44::InvertRotation(camera_pitch_matrix);
    Matrix44::InvertRotation(lean_back_matrix);
    Matrix44::InvertRotation(rotation_matrix);
    Matrix44::InvertTranslation(camera_position_matrix);
    Matrix44::InvertTranslation(camera_forward_matrix);
    Matrix44::InvertTranslation(free_look_matrix);
    Matrix44::InvertTranslation(head_position_matrix);

    look_matrix = rotation_matrix * head_position_matrix * lean_back_matrix * free_look_matrix *
                  camera_pitch_matrix * camera_position_matrix * camera_forward_matrix;
  }
  else
  {
    float HudWidth, HudHeight, HudThickness, HudDistance, HudUp, CameraForward, AimDistance;

    // 2D Screen
    if (!bHasWidest)
    {
      HudThickness = g_ActiveConfig.fScreenThickness * UnitsPerMetre;
      HudDistance = g_ActiveConfig.fScreenDistance * UnitsPerMetre;
      HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
      HudHeight = g_ActiveConfig.fScreenHeight * UnitsPerMetre;
      // NES games are supposed to be 1.175:1 (16:13.62) even though VC normally renders them as
      // 16:9
      // http://forums.nesdev.com/viewtopic.php?t=8063
      if (g_is_nes)
        HudWidth = HudHeight * 1.175f;
      else if (bAspectWide)
        HudWidth = HudHeight * (float)16 / 9;
      else
        HudWidth = HudHeight * (float)4 / 3;
      CameraForward = 0;
      HudUp = g_ActiveConfig.fScreenUp * UnitsPerMetre;
      AimDistance = HudDistance;
    }
    else
    // HUD over 3D world
    {
      // The HUD distance might have been carefully chosen to line up with objects, so we should
      // scale it with the world
      // But we can't make the HUD too close or it's hard to look at, and we should't make the HUD
      // too far or it stops looking 3D
      const float MinHudDistance = 0.28f, MaxHudDistance = 3.00f;  // HUD shouldn't go closer than
                                                                   // 28 cm when shrinking scale, or
                                                                   // further than 3m when growing
      float HUDScale = g_ActiveConfig.fScale;
      if (HUDScale < 1.0f && g_ActiveConfig.fHudDistance >= MinHudDistance &&
          g_ActiveConfig.fHudDistance * HUDScale < MinHudDistance)
        HUDScale = MinHudDistance / g_ActiveConfig.fHudDistance;
      else if (HUDScale > 1.0f && g_ActiveConfig.fHudDistance <= MaxHudDistance &&
               g_ActiveConfig.fHudDistance * HUDScale > MaxHudDistance)
        HUDScale = MaxHudDistance / g_ActiveConfig.fHudDistance;

      // Give the 2D layer a 3D effect if different parts of the 2D layer are rendered at different
      // z coordinates
      HudThickness = g_ActiveConfig.fHudThickness * HUDScale *
                     UnitsPerMetre;  // the 2D layer is actually a 3D box this many game units thick
      HudDistance = g_ActiveConfig.fHudDistance * HUDScale *
                    UnitsPerMetre;  // depth 0 on the HUD should be this far away

      HudUp = 0;
      if (bNoForward)
        CameraForward = 0;
      else
        CameraForward =
            (g_ActiveConfig.fCameraForward + zoom_forward) * g_ActiveConfig.fScale * UnitsPerMetre;
      // When moving the camera forward, correct the size of the HUD so that aiming is correct at
      // AimDistance
      AimDistance = g_ActiveConfig.fAimDistance * g_ActiveConfig.fScale * UnitsPerMetre;
      if (AimDistance <= 0)
        AimDistance = HudDistance;
      if (bShowAim)
      {
        HudThickness = 0;
        HudDistance = AimDistance;
        HUDScale = g_ActiveConfig.fScale;
      }
      // Now that we know how far away the box is, and what FOV it should fill, we can work out the
      // width and height in game units
      // Note: the HUD won't line up exactly (except at AimDistance) if CameraForward is non-zero
      // float HudWidth = 2.0f * tanf(hfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) * Correction;
      // float HudHeight = 2.0f * tanf(vfov / 2.0f * 3.14159f / 180.0f) * (HudDistance) *
      // Correction;
      HudWidth = 2.0f * tanf(DEGREES_TO_RADIANS(hfov / 2.0f)) * HudDistance *
                 (AimDistance + CameraForward) / AimDistance;
      HudHeight = 2.0f * tanf(DEGREES_TO_RADIANS(vfov / 2.0f)) * HudDistance *
                  (AimDistance + CameraForward) / AimDistance;
    }
    if (kind == 0 || bShowAim)
      HudThickness = HudWidth;

    float scale[3];  // width, height, and depth of box in game units divided by 2D width, height,
                     // and depth
    float position[3];  // position of front of box relative to the camera, in game units

    float viewport_scale[2];
    float viewport_offset[2];  // offset as a fraction of the viewport's width
    if (!bIsHudElement && !bIsOffscreen)
    {
      viewport_scale[0] = 1.0f;
      viewport_scale[1] = 1.0f;
      viewport_offset[0] = 0.0f;
      viewport_offset[1] = 0.0f;
    }
    else
    {
      Viewport& v = xfmem.viewport;
      float left, top, width, height;
      left = v.xOrig - v.wd - 342;
      top = v.yOrig + v.ht - 342;
      width = 2 * v.wd;
      height = -2 * v.ht;
      float screen_width = (float)g_final_screen_region.GetWidth();
      float screen_height = (float)g_final_screen_region.GetHeight();
      viewport_scale[0] = width / screen_width;
      viewport_scale[1] = height / screen_height;
      viewport_offset[0] = ((left + (width / 2)) - (0 + (screen_width / 2))) / screen_width;
      viewport_offset[1] = -((top + (height / 2)) - (0 + (screen_height / 2))) / screen_height;
    }

    // 3D HUD elements (may be part of 2D screen or HUD)
    if (bIsPerspective)
    {
      // these are the edges of the near clipping plane in game coordinates
      float left2D = 0;
      float right2D = 1;
      float bottom2D = 1;
      float top2D = 0;
      float zFar2D = 1;
      float zNear2D = -1;
      float zObj = zNear2D + (zFar2D - zNear2D) * g_ActiveConfig.fHud3DCloser;

      left2D *= zObj;
      right2D *= zObj;
      bottom2D *= zObj;
      top2D *= zObj;

      // Scale the width and height to fit the HUD in metres
      if (right2D == left2D)
      {
        scale[0] = 0;
      }
      else
      {
        scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
      }
      if (top2D == bottom2D)
      {
        scale[1] = 0;
      }
      else
      {
        scale[1] = viewport_scale[1] * HudHeight /
                   (top2D - bottom2D);  // note that positive means up in 3D
      }
      // Keep depth the same scale as width, so it looks like a real object
      if (zFar2D == zNear2D)
      {
        scale[2] = scale[0];
      }
      else
      {
        scale[2] = scale[0];
      }
      // Adjust the position for off-axis projection matrices, and shifting the 2D screen
      position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                    viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
      position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                    HudUp;  // shift it up into the centre of the view;
      // Shift it from the old near clipping plane to the HUD distance, and shift the camera forward
      if (!bHasWidest)
        position[2] = scale[2] * zObj - HudDistance;
      else
        position[2] = scale[2] * zObj - HudDistance;  // - CameraForward;
    }
    // 2D layer, or 2D viewport (may be part of 2D screen or HUD)
    else
    {
      float left2D = 0;
      float right2D = 1;
      float bottom2D = 1;
      float top2D = 0;
      float zNear2D = -1;
      float zFar2D = 1;

      // for 2D, work out the fraction of the HUD we should fill, and multiply the scale by that
      // also work out what fraction of the height we should shift it up, and what fraction of the
      // width we should shift it left
      // only multiply by the extra scale after adjusting the position?

      if (right2D == left2D)
      {
        scale[0] = 0;
      }
      else
      {
        scale[0] = viewport_scale[0] * HudWidth / (right2D - left2D);
      }
      if (top2D == bottom2D)
      {
        scale[1] = 0;
      }
      else
      {
        scale[1] = viewport_scale[1] * HudHeight /
                   (top2D - bottom2D);  // note that positive means up in 3D
      }
      if (zFar2D == zNear2D)
      {
        scale[2] = 0;  // The 2D layer was flat, so we make it flat instead of a box to avoid
                       // dividing by zero
      }
      else
      {
        scale[2] = HudThickness /
                   (zFar2D -
                    zNear2D);  // Scale 2D z values into 3D game units so it is the right thickness
      }
      position[0] = scale[0] * (-(right2D + left2D) / 2.0f) +
                    viewport_offset[0] * HudWidth;  // shift it right into the centre of the view
      position[1] = scale[1] * (-(top2D + bottom2D) / 2.0f) + viewport_offset[1] * HudHeight +
                    HudUp;  // shift it up into the centre of the view;
      // Shift it from the zero plane to the HUD distance, and shift the camera forward
      if (!bHasWidest)
        position[2] = -HudDistance;
      else
        position[2] = -HudDistance;  // - CameraForward;
    }

    Matrix44 scale_matrix, position_matrix;
    Matrix44::Scale(scale_matrix, scale);
    Matrix44::Translate(position_matrix, position);

    Matrix44::InvertScale(scale_matrix);
    Matrix44::InvertTranslation(position_matrix);
    Matrix44::InvertRotation(camera_pitch_matrix);
    Matrix44::InvertRotation(lean_back_matrix);
    Matrix44::InvertRotation(rotation_matrix);
    Matrix44::InvertTranslation(camera_position_matrix);
    Matrix44::InvertTranslation(free_look_matrix);
    Matrix44::InvertTranslation(head_position_matrix);

    // invert order: position, scale
    look_matrix = rotation_matrix * head_position_matrix * lean_back_matrix * free_look_matrix *
                  camera_pitch_matrix * camera_position_matrix * position_matrix * scale_matrix;
  }
  // done
  return true;
}
