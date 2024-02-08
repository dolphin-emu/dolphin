// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#define _USE_MATH_DEFINES
#include <cmath>

#include "Common/VR/DolphinVR.h"
#include "Common/VR/OpenXRLoader.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRMath.h"
#include "Common/VR/VRRenderer.h"

#include <cstdlib>
#include <cstring>

namespace Common::VR
{
void Renderer::GetResolution(engine_t* engine, int* pWidth, int* pHeight)
{
  static int width = 0;
  static int height = 0;

  if (engine)
  {
    // Enumerate the viewport configurations.
    uint32_t viewport_config_count = 0;
    OXR(xrEnumerateViewConfigurations(engine->app_state.instance, engine->app_state.system_id, 0,
                                      &viewport_config_count, NULL));

    XrViewConfigurationType* viewport_configs =
        (XrViewConfigurationType*)malloc(viewport_config_count * sizeof(XrViewConfigurationType));

    OXR(xrEnumerateViewConfigurations(engine->app_state.instance, engine->app_state.system_id,
                                      viewport_config_count, &viewport_config_count,
                                      viewport_configs));

    for (uint32_t i = 0; i < viewport_config_count; i++)
    {
      const XrViewConfigurationType viewport_config_type = viewport_configs[i];

      NOTICE_LOG_FMT(VR, "Viewport configuration type {}", (int)viewport_config_type);

      XrViewConfigurationProperties viewport_config;
      viewport_config.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
      OXR(xrGetViewConfigurationProperties(engine->app_state.instance, engine->app_state.system_id,
                                           viewport_config_type, &viewport_config));

      uint32_t view_count;
      OXR(xrEnumerateViewConfigurationViews(engine->app_state.instance, engine->app_state.system_id,
                                            viewport_config_type, 0, &view_count, NULL));

      if (view_count > 0)
      {
        XrViewConfigurationView* elements =
            (XrViewConfigurationView*)malloc(view_count * sizeof(XrViewConfigurationView));

        for (uint32_t e = 0; e < view_count; e++)
        {
          elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
          elements[e].next = NULL;
        }

        OXR(xrEnumerateViewConfigurationViews(engine->app_state.instance,
                                              engine->app_state.system_id, viewport_config_type,
                                              view_count, &view_count, elements));

        // Cache the view config properties for the selected config type.
        if (viewport_config_type == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        {
          assert(view_count == MaxNumEyes);
          for (uint32_t e = 0; e < view_count; e++)
          {
            engine->app_state.view_config[e] = elements[e];
          }
        }

        free(elements);
      }
      else
      {
        ERROR_LOG_FMT(VR, "Empty viewport configuration");
      }
    }

    free(viewport_configs);

    *pWidth = width = engine->app_state.view_config[0].recommendedImageRectWidth;
    *pHeight = height = engine->app_state.view_config[0].recommendedImageRectHeight;
  }
  else
  {
    // use cached values
    *pWidth = width;
    *pHeight = height;
  }
}

void Renderer::Init(engine_t* engine, bool multiview)
{
  if (m_initialized)
  {
    Destroy(engine);
  }

  int eyeW, eyeH;
  GetResolution(engine, &eyeW, &eyeH);
  SetConfigInt(CONFIG_VIEWPORT_WIDTH, eyeW);
  SetConfigInt(CONFIG_VIEWPORT_HEIGHT, eyeH);

  // Get the viewport configuration info for the chosen viewport configuration type.
  engine->app_state.viewport_config.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
  OXR(xrGetViewConfigurationProperties(engine->app_state.instance, engine->app_state.system_id,
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                       &engine->app_state.viewport_config));

  uint32_t num_spaces = 0;
  OXR(xrEnumerateReferenceSpaces(engine->app_state.session, 0, &num_spaces, NULL));
  auto* spaces = (XrReferenceSpaceType*)malloc(num_spaces * sizeof(XrReferenceSpaceType));
  OXR(xrEnumerateReferenceSpaces(engine->app_state.session, num_spaces, &num_spaces, spaces));

  for (uint32_t i = 0; i < num_spaces; i++)
  {
    if (spaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
    {
      m_stage_supported = true;
      break;
    }
  }

  free(spaces);

  if (engine->app_state.current_space == XR_NULL_HANDLE)
  {
    Recenter(engine);
  }

  m_projections = (XrView*)(malloc(MaxNumEyes * sizeof(XrView)));

  DisplayCreate(engine->app_state.session, &engine->app_state.renderer,
                engine->app_state.view_config[0].recommendedImageRectWidth,
                engine->app_state.view_config[0].recommendedImageRectHeight, multiview);
#ifdef ANDROID
  if (GetPlatformFlag(PLATFORM_EXTENSION_FOVEATION))
  {
    DisplaySetFoveation(&engine->app_state.instance, &engine->app_state.session,
                        &engine->app_state.renderer, XR_FOVEATION_LEVEL_HIGH_FB, 0,
                        XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB);
  }
#endif
  m_initialized = true;
}

void Renderer::Destroy(engine_t* engine)
{
  DisplayDestroy(&engine->app_state.renderer);
  free(m_projections);
  m_initialized = false;
}

bool Renderer::InitFrame(engine_t* engine)
{
  if (HandleXrEvents(&engine->app_state))
  {
    Recenter(engine);
  }
  if (!engine->app_state.session_active)
  {
    return false;
  }

  UpdateStageBounds(engine);

  // NOTE: OpenXR does not use the concept of frame indices. Instead,
  // XrWaitFrame returns the predicted display time.
  XrFrameWaitInfo wait_frame_info = {};
  wait_frame_info.type = XR_TYPE_FRAME_WAIT_INFO;
  wait_frame_info.next = NULL;

  m_frame_state.type = XR_TYPE_FRAME_STATE;
  m_frame_state.next = NULL;

  OXR(xrWaitFrame(engine->app_state.session, &wait_frame_info, &m_frame_state));
  engine->predicted_display_time = m_frame_state.predictedDisplayTime;

  // Get the HMD pose, predicted for the middle of the time period during which
  // the new eye images will be displayed. The number of frames predicted ahead
  // depends on the pipeline depth of the engine and the synthesis rate.
  // The better the prediction, the less black will be pulled in at the edges.
  XrFrameBeginInfo begin_frame_info = {};
  begin_frame_info.type = XR_TYPE_FRAME_BEGIN_INFO;
  begin_frame_info.next = NULL;
  OXR(xrBeginFrame(engine->app_state.session, &begin_frame_info));

  XrViewLocateInfo projection_info = {};
  projection_info.type = XR_TYPE_VIEW_LOCATE_INFO;
  projection_info.viewConfigurationType = engine->app_state.viewport_config.viewConfigurationType;
  projection_info.displayTime = m_frame_state.predictedDisplayTime;
  projection_info.space = engine->app_state.current_space;

  XrViewState view_state = {XR_TYPE_VIEW_STATE, NULL};

  uint32_t projection_capacity = MaxNumEyes;
  uint32_t projection_count = projection_capacity;

  OXR(xrLocateViews(engine->app_state.session, &projection_info, &view_state, projection_capacity,
                    &projection_count, m_projections));
  //

  m_fov = {};
  for (int eye = 0; eye < MaxNumEyes; eye++)
  {
    m_fov.angleLeft += m_projections[eye].fov.angleLeft / 2.0f;
    m_fov.angleRight += m_projections[eye].fov.angleRight / 2.0f;
    m_fov.angleUp += m_projections[eye].fov.angleUp / 2.0f;
    m_fov.angleDown += m_projections[eye].fov.angleDown / 2.0f;
    m_inverted_view_pose[eye] = m_projections[eye].pose;
  }

  m_hmd_orientation = EulerAngles(m_inverted_view_pose[0].orientation);
  engine->app_state.layer_count = 0;
  memset(engine->app_state.layers, 0, sizeof(CompositorLayer) * MaxLayerCount);
  return true;
}

void Renderer::BeginFrame(engine_t* engine, int fboIndex)
{
  m_config_int[CONFIG_CURRENT_FBO] = fboIndex;
  FramebufferAcquire(&engine->app_state.renderer.framebuffer[fboIndex]);
}

void Renderer::EndFrame(engine_t* engine)
{
  int fbo_index = m_config_int[CONFIG_CURRENT_FBO];
  BindFramebuffer(engine);

  // Show mouse cursor
  int mode = m_config_int[CONFIG_MODE];
  bool screen_mode = (mode == RENDER_MODE_MONO_SCREEN) || (mode == RENDER_MODE_STEREO_SCREEN);
  if (screen_mode && (m_config_int[CONFIG_MOUSE_SIZE] > 0))
  {
    int x = m_config_int[CONFIG_MOUSE_X];
    int y = m_config_int[CONFIG_MOUSE_Y];
    int sx = m_config_int[CONFIG_MOUSE_SIZE];
    int sy = (int)((float)sx * m_config_float[CONFIG_CANVAS_ASPECT]);
    DisplayMouseCursor(x, y, sx, sy);
  }

  FramebufferRelease(&engine->app_state.renderer.framebuffer[fbo_index]);
}

void Renderer::FinishFrame(engine_t* engine)
{
  int mode = m_config_int[CONFIG_MODE];
  XrCompositionLayerProjectionView projection_layer_elements[2] = {};
  if ((mode == RENDER_MODE_MONO_6DOF) || (mode == RENDER_MODE_STEREO_6DOF))
  {
    SetConfigFloat(CONFIG_MENU_YAW, m_hmd_orientation.y);

    for (int eye = 0; eye < MaxNumEyes; eye++)
    {
      int image_layer = engine->app_state.renderer.multiview ? eye : 0;
      Framebuffer* framebuffer = &engine->app_state.renderer.framebuffer[0];
      XrPosef pose = m_inverted_view_pose[0];
      if (mode != RENDER_MODE_MONO_6DOF)
      {
        if (!engine->app_state.renderer.multiview)
        {
          framebuffer = &engine->app_state.renderer.framebuffer[eye];
        }
        pose = m_inverted_view_pose[eye];
      }

      memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
      projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
      projection_layer_elements[eye].pose = pose;
      projection_layer_elements[eye].fov = m_fov;

      memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
      projection_layer_elements[eye].subImage.swapchain = framebuffer->swapchain_color.handle;
      projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
      projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
      projection_layer_elements[eye].subImage.imageRect.extent.width =
          framebuffer->swapchain_color.width;
      projection_layer_elements[eye].subImage.imageRect.extent.height =
          framebuffer->swapchain_color.height;
      projection_layer_elements[eye].subImage.imageArrayIndex = image_layer;
    }

    XrCompositionLayerProjection projection_layer = {};
    projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    projection_layer.space = engine->app_state.current_space;
    projection_layer.viewCount = MaxNumEyes;
    projection_layer.views = projection_layer_elements;

    engine->app_state.layers[engine->app_state.layer_count++].projection = projection_layer;
  }
  else if ((mode == RENDER_MODE_MONO_SCREEN) || (mode == RENDER_MODE_STEREO_SCREEN))
  {
    // Flat screen pose
    float distance = GetConfigFloat(CONFIG_CANVAS_DISTANCE);
    float menu_pitch = ToRadians(GetConfigFloat(CONFIG_MENU_PITCH));
    float menu_yaw = ToRadians(GetConfigFloat(CONFIG_MENU_YAW));
    XrVector3f pos = {m_inverted_view_pose[0].position.x - sinf(menu_yaw) * distance,
                      m_inverted_view_pose[0].position.y,
                      m_inverted_view_pose[0].position.z - cosf(menu_yaw) * distance};
    XrQuaternionf pitch = CreateFromVectorAngle({1, 0, 0}, -menu_pitch);
    XrQuaternionf yaw = CreateFromVectorAngle({0, 1, 0}, menu_yaw);

    // Setup the cylinder layer
    Framebuffer* framebuffer = &engine->app_state.renderer.framebuffer[0];
    XrCompositionLayerCylinderKHR cylinder_layer = {};
    cylinder_layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
    cylinder_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    cylinder_layer.space = engine->app_state.current_space;
    memset(&cylinder_layer.subImage, 0, sizeof(XrSwapchainSubImage));
    cylinder_layer.subImage.imageRect.offset.x = 0;
    cylinder_layer.subImage.imageRect.offset.y = 0;
    cylinder_layer.subImage.imageRect.extent.width = framebuffer->swapchain_color.width;
    cylinder_layer.subImage.imageRect.extent.height = framebuffer->swapchain_color.height;
    cylinder_layer.subImage.swapchain = framebuffer->swapchain_color.handle;
    cylinder_layer.subImage.imageArrayIndex = 0;
    cylinder_layer.pose.orientation = Multiply(pitch, yaw);
    cylinder_layer.pose.position = pos;
    cylinder_layer.radius = 6.0f;
    cylinder_layer.centralAngle = (float)(M_PI * 0.5);
    cylinder_layer.aspectRatio = GetConfigFloat(CONFIG_CANVAS_ASPECT);

    // Build the cylinder layer
    if (mode == RENDER_MODE_MONO_SCREEN)
    {
      cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
      engine->app_state.layers[engine->app_state.layer_count++].cylinder = cylinder_layer;
    }
    else if (engine->app_state.renderer.multiview)
    {
      cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
      engine->app_state.layers[engine->app_state.layer_count++].cylinder = cylinder_layer;
      cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
      cylinder_layer.subImage.imageArrayIndex = 1;
      engine->app_state.layers[engine->app_state.layer_count++].cylinder = cylinder_layer;
    }
    else
    {
      cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
      engine->app_state.layers[engine->app_state.layer_count++].cylinder = cylinder_layer;
      cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
      cylinder_layer.subImage.swapchain =
          engine->app_state.renderer.framebuffer[1].swapchain_color.handle;
      engine->app_state.layers[engine->app_state.layer_count++].cylinder = cylinder_layer;
    }
  }
  else
  {
    assert(false);
  }

  // Compose the layers for this frame.
  const XrCompositionLayerBaseHeader* layers[MaxLayerCount] = {};
  for (int i = 0; i < engine->app_state.layer_count; i++)
  {
    layers[i] = (const XrCompositionLayerBaseHeader*)&engine->app_state.layers[i];
  }

  XrFrameEndInfo end_frame_info = {};
  end_frame_info.type = XR_TYPE_FRAME_END_INFO;
  end_frame_info.displayTime = m_frame_state.predictedDisplayTime;
  end_frame_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  end_frame_info.layerCount = engine->app_state.layer_count;
  end_frame_info.layers = layers;

  OXR(xrEndFrame(engine->app_state.session, &end_frame_info));
  int instances = engine->app_state.renderer.multiview ? 1 : MaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
    Framebuffer* framebuffer = &engine->app_state.renderer.framebuffer[instances];
    framebuffer->swapchain_index++;
    framebuffer->swapchain_index %= framebuffer->swapchain_length;
  }
}

float Renderer::GetConfigFloat(ConfigFloat config)
{
  return m_config_float[config];
}

void Renderer::SetConfigFloat(ConfigFloat config, float value)
{
  m_config_float[config] = value;
}

int Renderer::GetConfigInt(ConfigInt config)
{
  return m_config_int[config];
}

void Renderer::SetConfigInt(ConfigInt config, int value)
{
  m_config_int[config] = value;
}

void Renderer::BindFramebuffer(engine_t* engine)
{
  if (!m_initialized)
    return;
  int fbo_index = GetConfigInt(CONFIG_CURRENT_FBO);
  FramebufferSetCurrent(&engine->app_state.renderer.framebuffer[fbo_index]);
}

XrView Renderer::GetView(int eye)
{
  return m_projections[eye];
}

XrVector3f Renderer::GetHMDAngles()
{
  return m_hmd_orientation;
}

void Renderer::Recenter(engine_t* engine)
{
  // Calculate recenter reference
  XrReferenceSpaceCreateInfo space_info = {};
  space_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  space_info.poseInReferenceSpace.orientation.w = 1.0f;
  if (engine->app_state.current_space != XR_NULL_HANDLE)
  {
    XrSpaceLocation loc = {};
    loc.type = XR_TYPE_SPACE_LOCATION;
    OXR(xrLocateSpace(engine->app_state.head_space, engine->app_state.current_space,
                      engine->predicted_display_time, &loc));
    m_hmd_orientation = EulerAngles(loc.pose.orientation);
    float yaw = m_hmd_orientation.y;

    SetConfigFloat(CONFIG_RECENTER_YAW, GetConfigFloat(CONFIG_RECENTER_YAW) + yaw);
    float renceter_yaw = ToRadians(GetConfigFloat(CONFIG_RECENTER_YAW));
    space_info.poseInReferenceSpace.orientation.x = 0;
    space_info.poseInReferenceSpace.orientation.y = sinf(renceter_yaw / 2);
    space_info.poseInReferenceSpace.orientation.z = 0;
    space_info.poseInReferenceSpace.orientation.w = cosf(renceter_yaw / 2);
  }

  // Delete previous space instances
  if (engine->app_state.stage_space != XR_NULL_HANDLE)
  {
    OXR(xrDestroySpace(engine->app_state.stage_space));
  }
  if (engine->app_state.fake_space != XR_NULL_HANDLE)
  {
    OXR(xrDestroySpace(engine->app_state.fake_space));
  }

  // Create a default stage space to use if SPACE_TYPE_STAGE is not
  // supported, or calls to xrGetReferenceSpaceBoundsRect fail.
  space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (GetPlatformFlag(PLATFORM_TRACKING_FLOOR))
  {
    space_info.poseInReferenceSpace.position.y = -1.6750f;
  }
  OXR(xrCreateReferenceSpace(engine->app_state.session, &space_info,
                             &engine->app_state.fake_space));
  NOTICE_LOG_FMT(VR, "Created fake stage space from local space with offset");
  engine->app_state.current_space = engine->app_state.fake_space;

  if (m_stage_supported)
  {
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    space_info.poseInReferenceSpace.position.y = 0.0;
    OXR(xrCreateReferenceSpace(engine->app_state.session, &space_info,
                               &engine->app_state.stage_space));
    NOTICE_LOG_FMT(VR, "Created stage space");
    if (GetPlatformFlag(PLATFORM_TRACKING_FLOOR))
    {
      engine->app_state.current_space = engine->app_state.stage_space;
    }
  }

  // Update menu orientation
  SetConfigFloat(CONFIG_MENU_PITCH, m_hmd_orientation.x);
  SetConfigFloat(CONFIG_MENU_YAW, 0.0f);
}

void Renderer::UpdateStageBounds(engine_t* engine)
{
  XrExtent2Df stage_bounds = {};

  XrResult result;
  OXR(result = xrGetReferenceSpaceBoundsRect(engine->app_state.session,
                                             XR_REFERENCE_SPACE_TYPE_STAGE, &stage_bounds));
  if (result != XR_SUCCESS)
  {
    stage_bounds.width = 1.0f;
    stage_bounds.height = 1.0f;

    engine->app_state.current_space = engine->app_state.fake_space;
  }
}
}  // namespace Common::VR
