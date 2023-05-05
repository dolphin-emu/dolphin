// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#define _USE_MATH_DEFINES
#include <cmath>

#include "Common/VR/Math.h"
#include "Common/VR/OpenXRLoader.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRRenderer.h"

#include <cstdlib>
#include <cstring>

XrFovf fov;
XrView* projections;
XrPosef invViewTransform[2];
XrFrameState frameState = {};
bool initialized = false;
bool stageSupported = false;
int vrConfig[VR_CONFIG_MAX] = {};
float vrConfigFloat[VR_CONFIG_FLOAT_MAX] = {};

XrVector3f hmdorientation;

void VR_UpdateStageBounds(App* pappState)
{
  XrExtent2Df stageBounds = {};

  XrResult result;
  OXR(result = xrGetReferenceSpaceBoundsRect(pappState->session, XR_REFERENCE_SPACE_TYPE_STAGE,
                                             &stageBounds));
  if (result != XR_SUCCESS)
  {
    ALOGV("Stage bounds query failed: using small defaults");
    stageBounds.width = 1.0f;
    stageBounds.height = 1.0f;

    pappState->current_space = pappState->fake_space;
  }

  ALOGV("Stage bounds: width = %f, depth %f", stageBounds.width, stageBounds.height);
}

void VR_GetResolution(engine_t* engine, int* pWidth, int* pHeight)
{
  static int width = 0;
  static int height = 0;

  if (engine)
  {
    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(engine->app_state.instance, engine->app_state.system_id, 0,
                                      &viewportConfigTypeCount, NULL));

    XrViewConfigurationType* viewportConfigurationTypes =
        (XrViewConfigurationType*)malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

    OXR(xrEnumerateViewConfigurations(engine->app_state.instance, engine->app_state.system_id,
                                      viewportConfigTypeCount, &viewportConfigTypeCount,
                                      viewportConfigurationTypes));

    ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

    for (uint32_t i = 0; i < viewportConfigTypeCount; i++)
    {
      const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

      ALOGV("Viewport configuration type %d : %s", viewportConfigType,
            viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO ? "Selected" : "");

      XrViewConfigurationProperties viewportConfig;
      viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
      OXR(xrGetViewConfigurationProperties(engine->app_state.instance, engine->app_state.system_id,
                                           viewportConfigType, &viewportConfig));
      ALOGV("FovMutable=%s ConfigurationType %d", viewportConfig.fovMutable ? "true" : "false",
            viewportConfig.viewConfigurationType);

      uint32_t viewCount;
      OXR(xrEnumerateViewConfigurationViews(engine->app_state.instance, engine->app_state.system_id,
                                            viewportConfigType, 0, &viewCount, NULL));

      if (viewCount > 0)
      {
        XrViewConfigurationView* elements =
            (XrViewConfigurationView*)malloc(viewCount * sizeof(XrViewConfigurationView));

        for (uint32_t e = 0; e < viewCount; e++)
        {
          elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
          elements[e].next = NULL;
        }

        OXR(xrEnumerateViewConfigurationViews(engine->app_state.instance, engine->app_state.system_id,
                                              viewportConfigType, viewCount, &viewCount, elements));

        // Cache the view config properties for the selected config type.
        if (viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        {
          assert(viewCount == MaxNumEyes);
          for (uint32_t e = 0; e < viewCount; e++)
          {
            engine->app_state.view_config[e] = elements[e];
          }
        }

        free(elements);
      }
      else
      {
        ALOGE("Empty viewport configuration type: %d", viewCount);
      }
    }

    free(viewportConfigurationTypes);

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

void VR_Recenter(engine_t* engine)
{
  // Calculate recenter reference
  XrReferenceSpaceCreateInfo spaceInfo = {};
  spaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
  if (engine->app_state.current_space != XR_NULL_HANDLE)
  {
    XrSpaceLocation loc = {};
    loc.type = XR_TYPE_SPACE_LOCATION;
    OXR(xrLocateSpace(engine->app_state.head_space, engine->app_state.current_space,
                      engine->predicted_display_time, &loc));
    hmdorientation = Common::VR::EulerAngles(loc.pose.orientation);
    float yaw = hmdorientation.y;

    VR_SetConfigFloat(VR_CONFIG_RECENTER_YAW, VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW) + yaw);
    float recenterYaw = Common::VR::ToRadians(VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW));
    spaceInfo.poseInReferenceSpace.orientation.x = 0;
    spaceInfo.poseInReferenceSpace.orientation.y = sinf(recenterYaw / 2);
    spaceInfo.poseInReferenceSpace.orientation.z = 0;
    spaceInfo.poseInReferenceSpace.orientation.w = cosf(recenterYaw / 2);
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
  spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR))
  {
    spaceInfo.poseInReferenceSpace.position.y = -1.6750f;
  }
  OXR(xrCreateReferenceSpace(engine->app_state.session, &spaceInfo, &engine->app_state.fake_space));
  ALOGV("Created fake stage space from local space with offset");
  engine->app_state.current_space = engine->app_state.fake_space;

  if (stageSupported)
  {
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceInfo.poseInReferenceSpace.position.y = 0.0;
    OXR(xrCreateReferenceSpace(engine->app_state.session, &spaceInfo, &engine->app_state.stage_space));
    ALOGV("Created stage space");
    if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR))
    {
      engine->app_state.current_space = engine->app_state.stage_space;
    }
  }

  // Update menu orientation
  VR_SetConfigFloat(VR_CONFIG_MENU_PITCH, hmdorientation.x);
  VR_SetConfigFloat(VR_CONFIG_MENU_YAW, 0.0f);
}

void VR_InitRenderer(engine_t* engine, bool multiview)
{
  if (initialized)
  {
    VR_DestroyRenderer(engine);
  }

  int eyeW, eyeH;
  VR_GetResolution(engine, &eyeW, &eyeH);
  VR_SetConfig(VR_CONFIG_VIEWPORT_WIDTH, eyeW);
  VR_SetConfig(VR_CONFIG_VIEWPORT_HEIGHT, eyeH);

  // Get the viewport configuration info for the chosen viewport configuration type.
  engine->app_state.viewport_config.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
  OXR(xrGetViewConfigurationProperties(engine->app_state.instance, engine->app_state.system_id,
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                       &engine->app_state.viewport_config));

  uint32_t numSpaces = 0;
  OXR(xrEnumerateReferenceSpaces(engine->app_state.session, 0, &numSpaces, NULL));
  auto* referenceSpaces = (XrReferenceSpaceType*)malloc(numSpaces * sizeof(XrReferenceSpaceType));
  OXR(xrEnumerateReferenceSpaces(engine->app_state.session, numSpaces, &numSpaces, referenceSpaces));

  for (uint32_t i = 0; i < numSpaces; i++)
  {
    if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
    {
      stageSupported = true;
      break;
    }
  }

  free(referenceSpaces);

  if (engine->app_state.current_space == XR_NULL_HANDLE)
  {
    VR_Recenter(engine);
  }

  projections = (XrView*)(malloc(MaxNumEyes * sizeof(XrView)));

	Common::VR::RendererCreate(engine->app_state.session, &engine->app_state.renderer,
	                           engine->app_state.view_config[0].recommendedImageRectWidth,
	                           engine->app_state.view_config[0].recommendedImageRectHeight,
	                           multiview);
#ifdef ANDROID
  if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_FOVEATION))
  {
	  Common::VR::RendererSetFoveation(&engine->app_state.instance, &engine->app_state.session,
	                                   &engine->app_state.renderer, XR_FOVEATION_LEVEL_HIGH_TOP_FB,
	                                   0, XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB);
  }
#endif
  initialized = true;
}

void VR_DestroyRenderer(engine_t* engine)
{
  Common::VR::RendererDestroy(&engine->app_state.renderer);
  free(projections);
  initialized = false;
}

bool VR_InitFrame(engine_t* engine)
{
  bool stageBoundsDirty = true;
  if (Common::VR::HandleXrEvents(&engine->app_state))
  {
    VR_Recenter(engine);
  }
  if (engine->app_state.session_active == false)
  {
    return false;
  }

  if (stageBoundsDirty)
  {
    VR_UpdateStageBounds(&engine->app_state);
    stageBoundsDirty = false;
  }

  // NOTE: OpenXR does not use the concept of frame indices. Instead,
  // XrWaitFrame returns the predicted display time.
  XrFrameWaitInfo waitFrameInfo = {};
  waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
  waitFrameInfo.next = NULL;

  frameState.type = XR_TYPE_FRAME_STATE;
  frameState.next = NULL;

  OXR(xrWaitFrame(engine->app_state.session, &waitFrameInfo, &frameState));
  engine->predicted_display_time = frameState.predictedDisplayTime;

  // Get the HMD pose, predicted for the middle of the time period during which
  // the new eye images will be displayed. The number of frames predicted ahead
  // depends on the pipeline depth of the engine and the synthesis rate.
  // The better the prediction, the less black will be pulled in at the edges.
  XrFrameBeginInfo beginFrameDesc = {};
  beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
  beginFrameDesc.next = NULL;
  OXR(xrBeginFrame(engine->app_state.session, &beginFrameDesc));

  XrViewLocateInfo projectionInfo = {};
  projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
  projectionInfo.viewConfigurationType = engine->app_state.viewport_config.viewConfigurationType;
  projectionInfo.displayTime = frameState.predictedDisplayTime;
  projectionInfo.space = engine->app_state.current_space;

  XrViewState viewState = {XR_TYPE_VIEW_STATE, NULL};

  uint32_t projectionCapacityInput = MaxNumEyes;
  uint32_t projectionCountOutput = projectionCapacityInput;

  OXR(xrLocateViews(engine->app_state.session, &projectionInfo, &viewState, projectionCapacityInput,
                    &projectionCountOutput, projections));
  //

  fov = {};
  for (int eye = 0; eye < MaxNumEyes; eye++)
  {
    fov.angleLeft += projections[eye].fov.angleLeft / 2.0f;
    fov.angleRight += projections[eye].fov.angleRight / 2.0f;
    fov.angleUp += projections[eye].fov.angleUp / 2.0f;
    fov.angleDown += projections[eye].fov.angleDown / 2.0f;
    invViewTransform[eye] = projections[eye].pose;
  }

  hmdorientation = Common::VR::EulerAngles(invViewTransform[0].orientation);
  engine->app_state.layer_count = 0;
  memset(engine->app_state.layers, 0, sizeof(CompositorLayer) * MaxLayerCount);
  return true;
}

void VR_BeginFrame(engine_t* engine, int fboIndex)
{
  vrConfig[VR_CONFIG_CURRENT_FBO] = fboIndex;
  Common::VR::FramebufferAcquire(&engine->app_state.renderer.framebuffer[fboIndex]);
}

void VR_EndFrame(engine_t* engine)
{
  int fboIndex = vrConfig[VR_CONFIG_CURRENT_FBO];
  VR_BindFramebuffer(engine);

  // Show mouse cursor
  int vrMode = vrConfig[VR_CONFIG_MODE];
  bool screenMode = (vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN);
  if (screenMode && (vrConfig[VR_CONFIG_MOUSE_SIZE] > 0))
  {
    int x = vrConfig[VR_CONFIG_MOUSE_X];
    int y = vrConfig[VR_CONFIG_MOUSE_Y];
    int sx = vrConfig[VR_CONFIG_MOUSE_SIZE];
    int sy = (int)((float)sx * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT));
	Common::VR::RendererMouseCursor(x, y, sx, sy);
  }

  Common::VR::FramebufferRelease(&engine->app_state.renderer.framebuffer[fboIndex]);
}

void VR_FinishFrame(engine_t* engine)
{
  int vrMode = vrConfig[VR_CONFIG_MODE];
  XrCompositionLayerProjectionView projection_layer_elements[2] = {};
  if ((vrMode == VR_MODE_MONO_6DOF) || (vrMode == VR_MODE_STEREO_6DOF))
  {
    VR_SetConfigFloat(VR_CONFIG_MENU_YAW, hmdorientation.y);

    for (int eye = 0; eye < MaxNumEyes; eye++)
    {
      int imageLayer = engine->app_state.renderer.multiview ? eye : 0;
      Framebuffer* frameBuffer = &engine->app_state.renderer.framebuffer[0];
      XrPosef pose = invViewTransform[0];
      if (vrMode != VR_MODE_MONO_6DOF)
      {
        if (!engine->app_state.renderer.multiview)
        {
          frameBuffer = &engine->app_state.renderer.framebuffer[eye];
        }
        pose = invViewTransform[eye];
      }

      memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
      projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
      projection_layer_elements[eye].pose = pose;
      projection_layer_elements[eye].fov = fov;

      memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
      projection_layer_elements[eye].subImage.swapchain = frameBuffer->swapchain_color.handle;
      projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
      projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
      projection_layer_elements[eye].subImage.imageRect.extent.width =
          frameBuffer->swapchain_color.width;
      projection_layer_elements[eye].subImage.imageRect.extent.height =
          frameBuffer->swapchain_color.height;
      projection_layer_elements[eye].subImage.imageArrayIndex = imageLayer;
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
  else if ((vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN))
  {
    // Flat screen pose
    float distance = VR_GetConfigFloat(VR_CONFIG_CANVAS_DISTANCE);
    float menuPitch = Common::VR::ToRadians(VR_GetConfigFloat(VR_CONFIG_MENU_PITCH));
    float menuYaw = Common::VR::ToRadians(VR_GetConfigFloat(VR_CONFIG_MENU_YAW));
    XrVector3f pos = {invViewTransform[0].position.x - sinf(menuYaw) * distance,
                      invViewTransform[0].position.y,
                      invViewTransform[0].position.z - cosf(menuYaw) * distance};
    XrQuaternionf pitch = Common::VR::CreateFromVectorAngle({1, 0, 0}, -menuPitch);
    XrQuaternionf yaw = Common::VR::CreateFromVectorAngle({0, 1, 0}, menuYaw);

    // Setup the cylinder layer
    Framebuffer* frameBuffer = &engine->app_state.renderer.framebuffer[0];
    XrCompositionLayerCylinderKHR cylinder_layer = {};
    cylinder_layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
    cylinder_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    cylinder_layer.space = engine->app_state.current_space;
    memset(&cylinder_layer.subImage, 0, sizeof(XrSwapchainSubImage));
    cylinder_layer.subImage.imageRect.offset.x = 0;
    cylinder_layer.subImage.imageRect.offset.y = 0;
    cylinder_layer.subImage.imageRect.extent.width = frameBuffer->swapchain_color.width;
    cylinder_layer.subImage.imageRect.extent.height = frameBuffer->swapchain_color.height;
    cylinder_layer.subImage.swapchain = frameBuffer->swapchain_color.handle;
    cylinder_layer.subImage.imageArrayIndex = 0;
    cylinder_layer.pose.orientation = Common::VR::Multiply(pitch, yaw);
    cylinder_layer.pose.position = pos;
    cylinder_layer.radius = 6.0f;
    cylinder_layer.centralAngle = (float)(M_PI * 0.5);
    cylinder_layer.aspectRatio = VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);

    // Build the cylinder layer
    if (vrMode == VR_MODE_MONO_SCREEN)
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

  XrFrameEndInfo endFrameInfo = {};
  endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
  endFrameInfo.displayTime = frameState.predictedDisplayTime;
  endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  endFrameInfo.layerCount = engine->app_state.layer_count;
  endFrameInfo.layers = layers;

  OXR(xrEndFrame(engine->app_state.session, &endFrameInfo));
  int instances = engine->app_state.renderer.multiview ? 1 : MaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
    Framebuffer* frameBuffer = &engine->app_state.renderer.framebuffer[instances];
    frameBuffer->swapchain_index++;
    frameBuffer->swapchain_index %= frameBuffer->swapchain_length;
  }
}

int VR_GetConfig(VRConfig config)
{
  return vrConfig[config];
}

void VR_SetConfig(VRConfig config, int value)
{
  vrConfig[config] = value;
}

float VR_GetConfigFloat(VRConfigFloat config)
{
  return vrConfigFloat[config];
}

void VR_SetConfigFloat(VRConfigFloat config, float value)
{
  vrConfigFloat[config] = value;
}

void VR_BindFramebuffer(engine_t* engine)
{
  if (!initialized)
    return;
  int fboIndex = VR_GetConfig(VR_CONFIG_CURRENT_FBO);
  Common::VR::FramebufferSetCurrent(&engine->app_state.renderer.framebuffer[fboIndex]);
}

XrView VR_GetView(int eye)
{
  return projections[eye];
}

XrVector3f VR_GetHMDAngles()
{
  return hmdorientation;
}
