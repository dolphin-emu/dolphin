// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsEnhancementsViewController.h"

#import "Core/Config/GraphicsSettings.h"

#import "GraphicsSettingsUtils.h"

@interface GraphicsEnhancementsViewController ()

@end

@implementation GraphicsEnhancementsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  GSUSetInitialForTransitionCell(Config::GFX_EFB_SCALE, self.m_efb_scale_label);
  //GSUSetInitialForTransitionCellU32(Config::GFX_MSAA, self.m_anti_aliasing_label);
  GSUSetInitialForTransitionCell(Config::GFX_ENHANCE_MAX_ANISOTROPY, self.m_anisotropic_filtering_label);
  GSUSetInitialForBool(Config::GFX_HACK_COPY_EFB_SCALED, false, self.m_scaled_efb_switch, self.m_scaled_efb_label);
  GSUSetInitialForBool(Config::GFX_ENABLE_PIXEL_LIGHTING, false, self.m_per_pixel_switch, self.m_per_pixel_label);
  GSUSetInitialForBool(Config::GFX_ENHANCE_FORCE_FILTERING, false, self.m_texture_filtering_switch, self.m_texture_filtering_label);
  GSUSetInitialForBool(Config::GFX_WIDESCREEN_HACK, false, self.m_widescreen_switch, self.m_widescreen_label);
  GSUSetInitialForBool(Config::GFX_DISABLE_FOG, false, self.m_fog_switch, self.m_fog_label);
  GSUSetInitialForBool(Config::GFX_ENHANCE_FORCE_TRUE_COLOR, false, self.m_high_bit_color_switch, self.m_high_bit_color_label);
  GSUSetInitialForBool(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, false, self.m_copy_filter_switch, self.m_copy_filter_label);
  GSUSetInitialForBool(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, false, self.m_mipmap_switch, self.m_mipmap_label);
}

- (IBAction)ScaledEFBChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_HACK_COPY_EFB_SCALED, [self.m_scaled_efb_switch isOn]);
}

- (IBAction)TextureFilteringChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_FILTERING, [self.m_texture_filtering_switch isOn]);
}

- (IBAction)FogChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_DISABLE_FOG, [self.m_fog_switch isOn]);
}

- (IBAction)CopyFilterChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, [self.m_copy_filter_switch isOn]);
}

- (IBAction)PerPixelChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENABLE_PIXEL_LIGHTING, [self.m_per_pixel_switch isOn]);
}

- (IBAction)WidescreenHackChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_WIDESCREEN_HACK, [self.m_widescreen_switch isOn]);
}

- (IBAction)HighBitColorChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_FORCE_TRUE_COLOR, [self.m_high_bit_color_switch isOn]);
}

- (IBAction)MipmapChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, [self.m_mipmap_switch isOn]);
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  switch (indexPath.row)
  {
    case 0:
      GSUShowHelpWithString(@"Controls the rendering resolution.\n\nA high resolution greatly improves "
                            "visual quality, but also greatly increases GPU load and can cause issues in "
                            "certain games. Generally speaking, the lower the internal resolution, the "
                            "better performance will be.\n\nIf unsure, select Native.");
      break;
    /*case 1:
      GSUShowHelpWithString(@"Reduces the amount of aliasing caused by rasterizing 3D graphics, resulting "
                            "in smoother edges on objects. Increases GPU load and sometimes causes graphical "
                            "issues.\n\nSSAA is significantly more demanding than MSAA, but provides top quality "
                            "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
                            "effects, and textures.\n\nIf unsure, select None.");
      break;*/
    case 1:
      GSUShowHelpWithString(@"Enables anisotropic filtering, which enhances the visual quality of textures that "
                            "are at oblique viewing angles.\n\nMight cause issues in a small "
                            "number of games.\n\nIf unsure, select 1x.");
      break;
    case 2:
      GSUShowHelpWithString(@"Greatly increases the quality of textures generated using render-to-texture "
                            "effects.\n\nSlightly increases GPU load and causes relatively few graphical "
                            "issues. Raising the internal resolution will improve the effect of this setting. "
                            "\n\nIf unsure, leave this checked.");
      break;
    case 3:
      GSUShowHelpWithString(@"Filters all textures, including any that the game explicitly set as "
                            "unfiltered.\n\nMay improve quality of certain textures in some games, but "
                            "will cause issues in others.\n\nIf unsure, leave this unchecked.");
      break;
    case 4:
      GSUShowHelpWithString(@"Makes distant objects more visible by removing fog, thus increasing the overall "
                            "detail.\n\nDisabling fog will break some games which rely on proper fog "
                            "emulation.\n\nIf unsure, leave this unchecked.");
      break;
    case 5:
      GSUShowHelpWithString(@"Disables the blending of adjacent rows when copying the EFB. This is known in "
                            "some games as \"deflickering\" or \"smoothing\". \n\nDisabling the filter has no "
                            "effect on performance, but may result in a sharper image. Causes few "
                            "graphical issues.\n\nIf unsure, leave this checked.");
      break;
    case 6:
      GSUShowHelpWithString(@"Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.\n\nRarely "
                            "causes slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
      break;
    case 7:
      GSUShowHelpWithString(@"Forces the game to output graphics for any aspect ratio. Use with \"Aspect Ratio\" set to "
                            "\"Force 16:9\" to force 4:3-only games to run at 16:9.\n\nRarely produces good results and "
                            "often partially breaks graphics and game UIs. Unnecessary (and detrimental) if using any "
                            "AR/Gecko-code widescreen patches.\n\nIf unsure, leave this unchecked.");
      break;
    case 8:
      GSUShowHelpWithString(@"Forces the game to render the RGB color channels in 24-bit, thereby increasing "
                            "quality by reducing color banding.\n\nHas no impact on performance and causes "
                            "few graphical issues.\n\nIf unsure, leave this checked.");
      break;
    case 9:
      GSUShowHelpWithString(@"Enables detection of arbitrary mipmaps, which some games use for special distance-based "
                            "effects.\n\nMay have false positives that result in blurry textures at increased internal "
                            "resolution, such as in games that use very low resolution mipmaps. Disabling this can also "
                            "reduce stutter in games that frequently load new textures. This feature is not compatible "
                            "with GPU Texture Decoding.\n\nIf unsure, leave this checked.");
      break;
  }
}

@end
