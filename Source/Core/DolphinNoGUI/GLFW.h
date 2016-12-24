// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if HAVE_GLFW
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/State.h"

#include "DolphinNoGUI/Platform.h"

#include "VideoCommon/RenderBase.h"

static void OnError(int error, const char* description)
{
  fprintf(stderr, "GLFW: 0x%08x: %s\n", error, description);
}

static void OnWindowClose(GLFWwindow* window)
{
  s_shutdown_requested.Set();
}

static void OnWindowFocus(GLFWwindow* window, int focused)
{
  if (focused)
  {
    rendererHasFocus = true;
    if (SConfig::GetInstance().bHideCursor && Core::GetState() != Core::CORE_PAUSE)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  }
  else
  {
    rendererHasFocus = false;
    if (SConfig::GetInstance().bHideCursor)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

static void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action != GLFW_PRESS)
    return;

  if (key == GLFW_KEY_ESCAPE)
  {
    if (Core::GetState() == Core::CORE_RUN)
    {
      if (SConfig::GetInstance().bHideCursor)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      Core::SetState(Core::CORE_PAUSE);
    }
    else
    {
      if (SConfig::GetInstance().bHideCursor)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
      Core::SetState(Core::CORE_RUN);
    }
  }
  else if ((key == GLFW_KEY_ENTER) && (mods & GLFW_MOD_ALT))
  {
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if (monitor)
    {
      int last_window_width = SConfig::GetInstance().iRenderWindowWidth;
      int last_window_height = SConfig::GetInstance().iRenderWindowHeight;
      glfwSetWindowMonitor(window, nullptr, GLFW_DONT_CARE, GLFW_DONT_CARE, last_window_width,
                           last_window_height, GLFW_DONT_CARE);
    }
    else
    {
      monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
      glfwSetWindowMonitor(window, monitor, 0, 0, vidmode->width, vidmode->height,
                           vidmode->refreshRate);
    }
  }
  else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F8)
  {
    int slot_number = key - GLFW_KEY_F1 + 1;
    if (mods & GLFW_MOD_SHIFT)
      State::Save(slot_number);
    else
      State::Load(slot_number);
  }
  else if (key == GLFW_KEY_F9)
    Core::SaveScreenShot();
  else if (key == GLFW_KEY_F11)
    State::LoadLastSaved();
  else if (key == GLFW_KEY_F12)
  {
    if (mods & GLFW_MOD_SHIFT)
      State::UndoLoadState();
    else
      State::UndoSaveState();
  }
}

static void OnFramebufferSize(GLFWwindow* window, int width, int height)
{
  int last_window_width = SConfig::GetInstance().iRenderWindowWidth;
  int last_window_height = SConfig::GetInstance().iRenderWindowHeight;
  if (last_window_width != width || last_window_height != height)
  {
    last_window_width = width;
    last_window_height = height;

    // We call Renderer::ChangeSurface here to indicate the size has changed,
    // but pass the same window handle. This is needed for the Vulkan backend,
    // otherwise it cannot tell that the window has been resized on some drivers.
    if (g_renderer)
      g_renderer->ChangeSurface(s_window_handle);
  }
}

class PlatformGLFW : public Platform
{
private:
  GLFWwindow* window = nullptr;

public:
  void Init() override
  {
    glfwSetErrorCallback(OnError);

    if (!glfwInit())
    {
      PanicAlert("GLFW initialization failed");
      exit(1);
    }

#if 1
    // TODO: these should only be set in the GLInterface instead.
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

#else
    // TODO: reenable that for ¬OpenGL, even though it seems to work.
    // We want to manage our context ourself.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

    bool fullscreen = SConfig::GetInstance().bFullscreen;

    GLFWmonitor* monitor = nullptr;
    if (fullscreen)
      monitor = glfwGetPrimaryMonitor();

    window =
        glfwCreateWindow(SConfig::GetInstance().iRenderWindowWidth,
                         SConfig::GetInstance().iRenderWindowHeight, "Dolphin", monitor, nullptr);
    if (!window)
    {
      glfwTerminate();
      PanicAlert("GLFW window creation failed");
      exit(2);
    }

    glfwSetWindowCloseCallback(window, OnWindowClose);
    glfwSetWindowFocusCallback(window, OnWindowFocus);
    glfwSetKeyCallback(window, OnKey);
    glfwSetFramebufferSizeCallback(window, OnFramebufferSize);

    s_window_handle = static_cast<void*>(window);
  }

  void SetTitle(const std::string& string) override { glfwSetWindowTitle(window, string.c_str()); }
  void MainLoop() override
  {
    while (s_running.IsSet())
    {
      if (s_shutdown_requested.TestAndClear())
      {
        const auto& stm = WII_IPC_HLE_Interface::GetDeviceByName("/dev/stm/eventhook");
        if (!s_tried_graceful_shutdown.IsSet() && stm &&
            std::static_pointer_cast<CWII_IPC_HLE_Device_stm_eventhook>(stm)->HasHookInstalled())
        {
          ProcessorInterface::PowerButton_Tap();
          s_tried_graceful_shutdown.Set();
        }
        else
        {
          s_running.Clear();
        }
      }
      glfwPollEvents();
      Core::HostDispatchJobs();
      usleep(100000);
    }
  }

  void Shutdown() override
  {
    if (window)
      glfwDestroyWindow(window);
    glfwTerminate();
  }
};
#endif
