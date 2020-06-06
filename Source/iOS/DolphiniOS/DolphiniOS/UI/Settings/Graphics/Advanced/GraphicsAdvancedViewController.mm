// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsAdvancedViewController.h"

#import "Core/Config/GraphicsSettings.h"
#import "Core/Config/SYSCONFSettings.h"

#import "GraphicsSettingsUtils.h"

@interface GraphicsAdvancedViewController ()

@end

@implementation GraphicsAdvancedViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  GSUSetInitialForBool(Config::GFX_ENABLE_WIREFRAME, false, self.m_wireframe_switch, self.m_wireframe_label);
  GSUSetInitialForBool(Config::GFX_OVERLAY_STATS, false, self.m_statistics_switch, self.m_statistics_label);
  GSUSetInitialForBool(Config::GFX_TEXFMT_OVERLAY_ENABLE, false, self.m_format_overlay_switch, self.m_format_overlay_label);
  GSUSetInitialForBool(Config::GFX_ENABLE_VALIDATION_LAYER, false, self.m_enable_api_validation_switch, self.m_enable_api_validation_label);
  
  GSUSetInitialForBool(Config::GFX_DUMP_TEXTURES, false, self.m_dump_textures_switch, self.m_dump_textures_label);
  GSUSetInitialForBool(Config::GFX_HIRES_TEXTURES, false, self.m_load_custom_textures_switch, self.m_load_custom_textures_label);
  GSUSetInitialForBool(Config::GFX_CACHE_HIRES_TEXTURES, false, self.m_prefetch_custom_textures_switch, self.m_prefetch_custom_textures_label);
  GSUSetInitialForBool(Config::GFX_DUMP_EFB_TARGET, false, self.m_dump_efb_target_switch, self.m_dump_efb_target_label);
  GSUSetInitialForBool(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, false, self.m_disable_vram_copies_switch, self.m_disable_vram_copies_label);
  
  GSUSetInitialForBool(Config::GFX_CROP, false, self.m_crop_switch, self.m_crop_label);
  GSUSetInitialForBool(Config::SYSCONF_PROGRESSIVE_SCAN, false, self.m_progressive_scan_switch, self.m_progressive_scan_label);
  GSUSetInitialForBool(Config::GFX_BACKEND_MULTITHREADING, false, self.m_backend_multithreading_switch, self.m_backend_multithreading_label);
  
  GSUSetInitialForBool(Config::GFX_HACK_EFB_DEFER_INVALIDATION, false, self.m_defer_efb_switch, self.m_defer_efb_label);
}

- (IBAction)WireframeChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENABLE_WIREFRAME, [self.m_wireframe_switch isOn]);
}

- (IBAction)ShowStatisticsChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_OVERLAY_STATS, [self.m_statistics_switch isOn]);
}

- (IBAction)OverlayChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_TEXFMT_OVERLAY_ENABLE, [self.m_format_overlay_switch isOn]);
}

- (IBAction)ApiValidationChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENABLE_VALIDATION_LAYER, [self.m_enable_api_validation_switch isOn]);
}

- (IBAction)DumpTexturesChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_DUMP_TEXTURES, [self.m_dump_textures_switch isOn]);
}

- (IBAction)LoadTexturesChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, [self.m_load_custom_textures_switch isOn]);
}

- (IBAction)PrefetchTexturesChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_CACHE_HIRES_TEXTURES, [self.m_prefetch_custom_textures_switch isOn]);
}

- (IBAction)DumpEfbTargetChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_DUMP_EFB_TARGET, [self.m_dump_efb_target_switch isOn]);
}

- (IBAction)DisableVramCopiesChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, [self.m_disable_vram_copies_switch isOn]);
}

- (IBAction)CropChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_CROP, [self.m_crop_switch isOn]);
}

- (IBAction)ProgressiveScanChanged:(id)sender
{
  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, [self.m_progressive_scan_switch isOn]);
}

- (IBAction)BackendMultithreadingChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_BACKEND_MULTITHREADING, [self.m_backend_multithreading_switch isOn]);
}

- (IBAction)DeferCacheValidationChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_EFB_DEFER_INVALIDATION, [self.m_defer_efb_switch isOn]);
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  switch (indexPath.section)
  {
    case 0:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Renders the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Shows various rendering statistics.\n\nIf unsure, leave this unchecked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Modifies textures to show the format they're encoded in.\n\nMay require an emulation "
                                "reset to apply.\n\nIf unsure, leave this unchecked.");
          break;
        case 3:
          GSUShowHelpWithString(@"Enables validation of API calls made by the video backend, which may assist in "
                                "debugging graphical issues.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 1:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Dumps decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave "
                                "this unchecked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Loads custom textures from User/Load/Textures/<game_id>/.\n\nIf unsure, leave this "
                                "unchecked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Caches custom textures to system RAM on startup.\n\nThis can require exponentially "
                                "more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
          break;
        case 3:
          GSUShowHelpWithString(@"Dumps the contents of EFB copies to User/Dump/Textures/.\n\nIf unsure, leave this "
                                "unchecked.");
          break;
        case 4:
          GSUShowHelpWithString(@"Disables the VRAM copy of the EFB, forcing a round-trip to RAM. Inhibits all "
                                "upscaling.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 2:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Crops the picture from its native aspect ratio to 4:3 or "
                                "16:9.\n\nIf unsure, leave this unchecked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Enables progressive scan if supported by the emulated software. Most games don't have "
                                "any issue with this.\n\nIf unsure, leave this unchecked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Enables multithreaded command submission in backends where supported. Enabling "
                                "this option may result in a performance improvement on systems with more than "
                                "two CPU cores. Currently, this is limited to the Vulkan backend.\n\nIf unsure, "
                                "leave this checked.");
          break;
      }
      break;
    case 3:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Defers invalidation of the EFB access cache until a GPU synchronization command "
                                "is executed. If disabled, the cache will be invalidated with every draw call. "
                                "\n\nMay improve performance in some games which rely on CPU EFB Access at the cost "
                                "of stability.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
  }
}

@end
