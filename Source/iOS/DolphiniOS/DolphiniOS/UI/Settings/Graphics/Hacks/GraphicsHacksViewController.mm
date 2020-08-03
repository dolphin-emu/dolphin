// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsHacksViewController.h"

#import "Core/Config/GraphicsSettings.h"

#import "GraphicsSettingsUtils.h"

@interface GraphicsHacksViewController ()

@end

@implementation GraphicsHacksViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(bool)animated
{
  [super viewWillAppear:animated];
  
  GSUSetInitialForBool(Config::GFX_HACK_EFB_ACCESS_ENABLE, true, self.m_skip_efb_cpu_switch, self.m_skip_efb_cpu_label);
  GSUSetInitialForBool(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, true, self.m_ignore_format_changes_switch, self.m_ignore_format_changes_label);
  GSUSetInitialForBool(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, false, self.m_store_efb_copies_switch, self.m_store_efb_copies_label);
  GSUSetInitialForBool(Config::GFX_HACK_DEFER_EFB_COPIES, false, self.m_defer_efb_copies_switch, self.m_defer_efb_copies_label);
  
  GSUSetInitialForTransitionCell(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, self.m_accuracy_label);
  switch (Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES))
  {
    case 512:
      [self.m_accuracy_slider setValue:1];
      break;
    case 128:
      [self.m_accuracy_slider setValue:2];
      break;
    case 0:
      [self.m_accuracy_slider setValue:0];
      break;
    default:
      [self.m_accuracy_slider setEnabled:false];
  }
  
  GSUSetInitialForBool(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, false, self.m_gpu_decoding_switch, self.m_gpu_decoding_label);
  
  GSUSetInitialForBool(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, false, self.m_store_xfb_copies_switch, self.m_store_xfb_copies_label);
  GSUSetInitialForBool(Config::GFX_HACK_IMMEDIATE_XFB, false, self.m_immediate_xfb_switch, self.m_immediate_xfb_label);
  
  GSUSetInitialForBool(Config::GFX_FAST_DEPTH_CALC, false, self.m_fast_depth_switch, self.m_fast_depth_label);
  GSUSetInitialForBool(Config::GFX_HACK_BBOX_ENABLE, true, self.m_bbox_switch, self.m_bbox_label);
  GSUSetInitialForBool(Config::GFX_HACK_VERTEX_ROUDING, false, self.m_vertex_rounding_switch, self.m_vertex_rounding_label);
  GSUSetInitialForBool(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE, false, self.m_save_texture_cache_switch, self.m_save_texture_cache_label);
  
  [self UpdateDeferEfbCopiesEnabled];
}

- (void)UpdateDeferEfbCopiesEnabled
{
  const bool can_defer = [self.m_store_efb_copies_switch isOn] && [self.m_store_xfb_copies_switch isOn];
  [self.m_defer_efb_copies_switch setEnabled:!can_defer];
}

- (IBAction)SkipEfbChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_EFB_ACCESS_ENABLE, ![self.m_skip_efb_cpu_switch isOn]);
}

- (IBAction)IgnoreFormatChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, ![self.m_ignore_format_changes_switch isOn]);
}

- (IBAction)SkipEfbToRamChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, [self.m_store_efb_copies_switch isOn]);
  [self UpdateDeferEfbCopiesEnabled];
}

- (IBAction)DeferEfbChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_DEFER_EFB_COPIES, [self.m_defer_efb_copies_switch isOn]);
}

- (IBAction)AccuracyChanged:(id)sender
{
  int value = (int)[self.m_accuracy_slider value];
  
  int samples = 0;
  switch (value)
  {
    case 0:
      samples = 0;
      break;
    case 1:
      samples = 512;
      break;
    case 2:
      samples = 128;
      break;
  }
  
  Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, samples);
  
  [self.m_accuracy_slider setValue:(float)value];
}

- (IBAction)GpuTextureDecodingChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, [self.m_gpu_decoding_switch isOn]);
}

- (IBAction)StoreXfbChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, [self.m_store_xfb_copies_switch isOn]);
  [self UpdateDeferEfbCopiesEnabled];
}
- (IBAction)ImmediateXfbChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_IMMEDIATE_XFB, [self.m_immediate_xfb_switch isOn]);
}

- (IBAction)FastDepthChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_FAST_DEPTH_CALC, [self.m_fast_depth_switch isOn]);
}

- (IBAction)BBoxChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_BBOX_ENABLE, ![self.m_bbox_switch isOn]);
}

- (IBAction)VertexRoundingChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_VERTEX_ROUDING, [self.m_vertex_rounding_switch isOn]);
}

- (IBAction)SaveTextureCacheChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE, [self.m_save_texture_cache_switch isOn]);
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  switch (indexPath.section)
  {
    case 0:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Ignores any requests from the CPU to read from or write to the EFB. "
                                "\n\nImproves performance in some games, but will disable all EFB-based "
                                "graphical effects or gameplay-related features.\n\nIf unsure, "
                                "leave this unchecked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Ignores any changes to the EFB format.\n\nImproves performance in many games without "
                                "any negative effect. Causes graphical defects in a small number of other "
                                "games.\n\nIf unsure, leave this checked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Stores EFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
                                "in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to "
                                "RAM (and Texture)\n\nIf unsure, leave this checked.");
          break;
        case 3:
          GSUShowHelpWithString(@"Waits until the game synchronizes with the emulated GPU before writing the contents of EFB "
                                "copies to RAM.\n\nReduces the overhead of EFB RAM copies, providing a performance boost in "
                                "many games, at the risk of breaking those which do not safely synchronize with the "
                                "emulated GPU.\n\nIf unsure, leave this checked.");
          break;
      }
      break;
    case 1:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Adjusts the accuracy at which the GPU receives texture updates from RAM.\n\n"
                                "The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
                                "from RAM. Lower accuracies cause in-game text to appear garbled in certain "
                                "games.\n\nIf unsure, select the rightmost value.");
          break;
        case 2:
          GSUShowHelpWithString(@"Enables texture decoding using the GPU instead of the CPU.\n\nThis may result in "
                                "performance gains in some scenarios, or on systems where the CPU is the "
                                "bottleneck.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 2:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Stores XFB copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
                                "in a small number of games.\n\nEnabled = XFB Copies to "
                                "Texture\nDisabled = XFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Displays XFB copies as soon as they are created, instead of waiting for "
                                "scanout.\n\nCan cause graphical defects in some games if the game doesn't "
                                "expect all XFB copies to be displayed. However, turning this setting on reduces "
                                "latency.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 3:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Uses a less accurate algorithm to calculate depth values.\n\nCauses issues in a few "
                                "games, but can result in a decent speed increase depending on the game and/or "
                                "GPU.\n\nIf unsure, leave this checked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Disables bounding box emulation.\n\nThis may improve GPU performance "
                                "significantly, but some games will break.\n\nIf unsure, leave this checked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Rounds 2D vertices to whole pixels.\n\nFixes graphical problems in some games at "
                                "higher internal resolutions. This setting has no effect when native internal "
                                "resolution is used.\n\nIf unsure, leave this unchecked.");
        case 3:
          GSUShowHelpWithString(@"Includes the contents of the embedded frame buffer (EFB) and upscaled EFB copies "
                                "in save states. Fixes missing and/or non-upscaled textures/objects when loading "
                                "states at the cost of additional save/load time.\n\nIf unsure, leave this checked.");
          break;
      }
      break;
  }
}

@end
