// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <GLFW/glfw3.h>
#include <array>
#include <cstdlib>

#include "Common/GL/GLInterface/GLFW.h"
#include "Common/Logging/Log.h"

void cInterfaceGLFW::SwapInterval(int interval)
{
  glfwSwapInterval(interval);
}

void cInterfaceGLFW::Swap()
{
  glfwSwapBuffers(window);
}

void* cInterfaceGLFW::GetFuncAddress(const std::string& name)
{
  return reinterpret_cast<void*>(glfwGetProcAddress(name.c_str()));
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLFW::Create(void* window_handle, bool core)
{
#if 0
  // attributes for a visual in RGBA format with at least
  // 8 bits per color
  glfwWindowHint(GLFW_RED_BITS, 8);
  glfwWindowHint(GLFW_GREEN_BITS, 8);
  glfwWindowHint(GLFW_BLUE_BITS, 8);

  if (s_opengl_mode == GLInterfaceMode::MODE_DETECT)
    s_opengl_mode = GLInterfaceMode::MODE_OPENGL;

  switch (s_opengl_mode)
  {
    case GLInterfaceMode::MODE_OPENGL:
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    break;
    case GLInterfaceMode::MODE_OPENGLES2:
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    break;
    case GLInterfaceMode::MODE_OPENGLES3:
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    break;
    default:
      ERROR_LOG(VIDEO, "Unknown opengl mode set\n");
      return false;
    break;
  }
#endif

  window = static_cast<GLFWwindow*>(window_handle);

  INFO_LOG(VIDEO, "GLFW version: %s", glfwGetVersionString());

  return true;
}

bool cInterfaceGLFW::Create(cInterfaceBase* main_context)
{
  // XXX
  abort();
  return false;
}

bool cInterfaceGLFW::MakeCurrent()
{
  glfwMakeContextCurrent(window);
  return true;
}

bool cInterfaceGLFW::ClearCurrent()
{
  glfwMakeContextCurrent(nullptr);
  return true;
}

void cInterfaceGLFW::Shutdown()
{
}

void cInterfaceGLFW::Update()
{
  int width, height;

  glfwGetFramebufferSize(window, &width, &height);

  s_backbuffer_width = width;
  s_backbuffer_height = height;
}

bool cInterfaceGLFW::PeekMessages()
{
  // abort();
  return true;
}

std::unique_ptr<cInterfaceBase> cInterfaceGLFW::CreateSharedContext()
{
  abort();
}
