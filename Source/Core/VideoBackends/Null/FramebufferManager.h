// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManagerBase.h"

class XFBSource : public XFBSourceBase
{
public:
  void DecodeToTexture(u32 xfb_addr, u32 fb_width, u32 fb_height) override {}
  void CopyEFB(float gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
public:
  std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width,
                                                 unsigned int target_height,
                                                 unsigned int layers) override
  {
    return std::make_unique<XFBSource>();
  }

  void GetTargetSize(unsigned int* width, unsigned int* height) override {}
  void CopyToRealXFB(u32 xfb_addr, u32 fb_stride, u32 fb_height, const EFBRectangle& source_rc,
                     float gamma = 1.0f) override
  {
  }
};
