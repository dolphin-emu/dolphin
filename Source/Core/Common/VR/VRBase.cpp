// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/VR/VRBase.h"
#include "Common/VR/DolphinVR.h"
#include "Common/VR/VRDisplay.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace Common::VR
{
void Base::Init(void* system, const char* name, int version)
{
  if (m_initialized)
    return;

  if (!XRLoad())
  {
    return;
  }

  AppClear(&m_engine.app_state);

#ifdef ANDROID
  PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                        (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
  if (xrInitializeLoaderKHR != NULL)
  {
    vrJava* java = (vrJava*)system;
    XrLoaderInitInfoAndroidKHR loader_info;
    memset(&loader_info, 0, sizeof(loader_info));
    loader_info.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
    loader_info.next = NULL;
    loader_info.applicationVM = java->vm;
    loader_info.applicationContext = java->activity;
    xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loader_info);
  }
#endif

  std::vector<const char*> extensions;
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
#endif
  extensions.push_back(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
#ifdef ANDROID
  if (GetPlatformFlag(PLATFORM_EXTENSION_FOVEATION))
  {
    extensions.push_back(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME);
    extensions.push_back(XR_FB_SWAPCHAIN_UPDATE_STATE_OPENGL_ES_EXTENSION_NAME);
    extensions.push_back(XR_FB_FOVEATION_EXTENSION_NAME);
    extensions.push_back(XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME);
  }
  if (GetPlatformFlag(PLATFORM_EXTENSION_INSTANCE))
  {
    extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  }
  if (GetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE))
  {
    extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
    extensions.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
  }
#endif

  // Create the OpenXR instance.
  XrApplicationInfo app_info;
  memset(&app_info, 0, sizeof(app_info));
  strcpy(app_info.applicationName, name);
  strcpy(app_info.engineName, name);
  app_info.applicationVersion = version;
  app_info.engineVersion = version;
  app_info.apiVersion = XR_CURRENT_API_VERSION;

  XrInstanceCreateInfo instance_info;
  memset(&instance_info, 0, sizeof(instance_info));
  instance_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
  instance_info.next = NULL;
  instance_info.createFlags = 0;
  instance_info.applicationInfo = app_info;
  instance_info.enabledApiLayerCount = 0;
  instance_info.enabledApiLayerNames = NULL;
  instance_info.enabledExtensionCount = (uint32_t)extensions.size();
  instance_info.enabledExtensionNames = extensions.data();

#ifdef ANDROID
  XrInstanceCreateInfoAndroidKHR instance_info_android = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  if (GetPlatformFlag(PLATFORM_EXTENSION_INSTANCE))
  {
    vrJava* java = (vrJava*)system;
    instance_info_android.applicationVM = java->vm;
    instance_info_android.applicationActivity = java->activity;
    instance_info.next = (XrBaseInStructure*)&instance_info_android;
  }
#endif

  XrResult result;
  OXR(result = xrCreateInstance(&instance_info, &m_engine.app_state.instance));
  if (result != XR_SUCCESS)
  {
    ERROR_LOG_FMT(VR, "Failed to create XR instance: {}", (int)result);
    exit(1);
  }

  XRLoadInstanceFunctions(m_engine.app_state.instance);

  XrInstanceProperties instance_properties;
  instance_properties.type = XR_TYPE_INSTANCE_PROPERTIES;
  instance_properties.next = NULL;
  OXR(xrGetInstanceProperties(m_engine.app_state.instance, &instance_properties));
  NOTICE_LOG_FMT(VR, "Runtime {}: Version : {}.{}.{}", instance_properties.runtimeName,
                 XR_VERSION_MAJOR(instance_properties.runtimeVersion),
                 XR_VERSION_MINOR(instance_properties.runtimeVersion),
                 XR_VERSION_PATCH(instance_properties.runtimeVersion));

  XrSystemGetInfo system_info;
  memset(&system_info, 0, sizeof(system_info));
  system_info.type = XR_TYPE_SYSTEM_GET_INFO;
  system_info.next = NULL;
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  XrSystemId system_id;
  OXR(result = xrGetSystem(m_engine.app_state.instance, &system_info, &system_id));
  if (result != XR_SUCCESS)
  {
    ERROR_LOG_FMT(VR, "Failed to get system");
    exit(1);
  }

  // Get the graphics requirements.
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
  OXR(xrGetInstanceProcAddr(m_engine.app_state.instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                            (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

  XrGraphicsRequirementsOpenGLESKHR graphics_requirements = {};
  graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
  OXR(pfnGetOpenGLESGraphicsRequirementsKHR(m_engine.app_state.instance, system_id,
                                            &graphics_requirements));
#endif

#ifdef ANDROID
  m_engine.app_state.main_thread_id = gettid();
#endif
  m_engine.app_state.system_id = system_id;
  m_initialized = true;
}

void Base::Destroy(engine_t* engine)
{
  if (engine == &m_engine)
  {
    xrDestroyInstance(engine->app_state.instance);
    Common::VR::AppDestroy(&engine->app_state);
    m_initialized = false;
  }
}

void Base::EnterVR(engine_t* engine)
{
  if (engine->app_state.session)
  {
    ERROR_LOG_FMT(VR, "EnterVR called with existing session");
    return;
  }

  // Create the OpenXR Session.
  XrSessionCreateInfo session_info = {};
#ifdef ANDROID
  XrGraphicsBindingOpenGLESAndroidKHR graphics_binding_gl = {};
#elif XR_USE_GRAPHICS_API_OPENGL
  XrGraphicsBindingOpenGLWin32KHR graphicsBindingGL = {};
#endif
  memset(&session_info, 0, sizeof(session_info));
#ifdef ANDROID
  graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
  graphics_binding_gl.next = NULL;
  graphics_binding_gl.display = eglGetCurrentDisplay();
  graphics_binding_gl.config = NULL;
  graphics_binding_gl.context = eglGetCurrentContext();
  session_info.next = &graphics_binding_gl;
#else
  // TODO:PCVR definition
#endif
  session_info.type = XR_TYPE_SESSION_CREATE_INFO;
  session_info.createFlags = 0;
  session_info.systemId = engine->app_state.system_id;

  XrResult result;
  OXR(result =
          xrCreateSession(engine->app_state.instance, &session_info, &engine->app_state.session));
  if (result != XR_SUCCESS)
  {
    ERROR_LOG_FMT(VR, "Failed to create XR session: {}", (int)result);
    exit(1);
  }

  // Create a space to the first path
  XrReferenceSpaceCreateInfo space_info = {};
  space_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  space_info.poseInReferenceSpace.orientation.w = 1.0f;
  OXR(xrCreateReferenceSpace(engine->app_state.session, &space_info,
                             &engine->app_state.head_space));
}

void Base::LeaveVR(engine_t* engine)
{
  if (engine->app_state.session)
  {
    OXR(xrDestroySpace(engine->app_state.head_space));
    // StageSpace is optional.
    if (engine->app_state.stage_space != XR_NULL_HANDLE)
    {
      OXR(xrDestroySpace(engine->app_state.stage_space));
    }
    OXR(xrDestroySpace(engine->app_state.fake_space));
    engine->app_state.current_space = XR_NULL_HANDLE;
    OXR(xrDestroySession(engine->app_state.session));
    engine->app_state.session = XR_NULL_HANDLE;
  }
}

engine_t* Base::GetEngine()
{
  return &m_engine;
}
}  // namespace Common::VR
