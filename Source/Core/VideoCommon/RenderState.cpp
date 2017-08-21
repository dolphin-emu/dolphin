// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/RenderState.h"

// If the framebuffer format has no alpha channel, it is assumed to
// ONE on blending. As the backends may emulate this framebuffer
// configuration with an alpha channel, we just drop all references
// to the destination alpha channel.
static BlendMode::BlendFactor RemoveDstAlphaUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::DSTALPHA:
    return BlendMode::ONE;
  case BlendMode::INVDSTALPHA:
    return BlendMode::ZERO;
  default:
    return factor;
  }
}

// We separate the blending parameter for rgb and alpha. For blending
// the alpha component, CLR and ALPHA are indentical. So just always
// use ALPHA as this makes it easier for the backends to use the second
// alpha value of dual source blending.
static BlendMode::BlendFactor RemoveSrcColorUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::SRCCLR:
    return BlendMode::SRCALPHA;
  case BlendMode::INVSRCCLR:
    return BlendMode::INVSRCALPHA;
  default:
    return factor;
  }
}

// Same as RemoveSrcColorUsage, but because of the overlapping enum,
// this must be written as another function.
static BlendMode::BlendFactor RemoveDstColorUsage(BlendMode::BlendFactor factor)
{
  switch (factor)
  {
  case BlendMode::DSTCLR:
    return BlendMode::DSTALPHA;
  case BlendMode::INVDSTCLR:
    return BlendMode::INVDSTALPHA;
  default:
    return factor;
  }
}

void BlendingState::Generate(const BPMemory& bp)
{
  // Start with everything disabled.
  hex = 0;

  bool target_has_alpha = bp.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  bool alpha_test_may_success = bp.alpha_test.TestResult() != AlphaTest::FAIL;

  colorupdate = bp.blendmode.colorupdate && alpha_test_may_success;
  alphaupdate = bp.blendmode.alphaupdate && target_has_alpha && alpha_test_may_success;
  dstalpha = bp.dstalpha.enable && alphaupdate;
  usedualsrc = true;

  // The subtract bit has the highest priority
  if (bp.blendmode.subtract)
  {
    blendenable = true;
    subtractAlpha = subtract = true;
    srcfactoralpha = srcfactor = BlendMode::ONE;
    dstfactoralpha = dstfactor = BlendMode::ONE;

    if (dstalpha)
    {
      subtractAlpha = false;
      srcfactoralpha = BlendMode::ONE;
      dstfactoralpha = BlendMode::ZERO;
    }
  }

  // The blendenable bit has the middle priority
  else if (bp.blendmode.blendenable)
  {
    blendenable = true;
    srcfactor = bp.blendmode.srcfactor;
    dstfactor = bp.blendmode.dstfactor;
    if (!target_has_alpha)
    {
      // uses ONE instead of DSTALPHA
      srcfactor = RemoveDstAlphaUsage(srcfactor);
      dstfactor = RemoveDstAlphaUsage(dstfactor);
    }
    // replaces SRCCLR with SRCALPHA and DSTCLR with DSTALPHA, it is important to
    // use the dst function for the src factor and vice versa
    srcfactoralpha = RemoveDstColorUsage(srcfactor);
    dstfactoralpha = RemoveSrcColorUsage(dstfactor);

    if (dstalpha)
    {
      srcfactoralpha = BlendMode::ONE;
      dstfactoralpha = BlendMode::ZERO;
    }
  }

  // The logicop bit has the lowest priority
  else if (bp.blendmode.logicopenable)
  {
    if (bp.blendmode.logicmode == BlendMode::NOOP)
    {
      // Fast path for Kirby's Return to Dreamland, they use it with dstAlpha.
      colorupdate = false;
      alphaupdate = alphaupdate && dstalpha;
    }
    else
    {
      logicopenable = true;
      logicmode = bp.blendmode.logicmode;

      if (dstalpha)
      {
        // TODO: Not supported by backends.
      }
    }
  }
}
