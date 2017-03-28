// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLInterface/EGL.h"

class cInterfaceEGLHaiku final : public cInterfaceEGL
{
protected:
  EGLDisplay OpenDisplay() override;
  EGLNativeWindowType InitializePlatform(EGLNativeWindowType host_window,
                                         EGLConfig config) override;
  void ShutdownPlatform() override;
};
