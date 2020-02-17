// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsGeneralViewController.h"

#import "Core/Config/GraphicsSettings.h"

#import "GraphicsSettingsUtils.h"

@interface GraphicsGeneralViewController ()

@end

@implementation GraphicsGeneralViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  GSUSetInitialForTransitionCell(Config::GFX_ASPECT_RATIO, self.m_aspect_ratio_label);
  GSUSetInitialForBool(Config::GFX_VSYNC, false, self.m_vsync_switch, self.m_vsync_label);
  
  GSUSetInitialForBool(Config::GFX_SHOW_FPS, false, self.m_fps_switch, self.m_fps_label);
  GSUSetInitialForBool(Config::GFX_SHOW_NETPLAY_PING, false, self.m_netplay_ping_switch, self.m_netplay_ping_label);
  GSUSetInitialForBool(Config::GFX_LOG_RENDER_TIME_TO_FILE, false, self.m_render_time_log_switch, self.m_render_time_log_label);
  GSUSetInitialForBool(Config::GFX_SHOW_NETPLAY_MESSAGES, false, self.m_netplay_messages_switch, self.m_netplay_messages_label);
  
  GSUSetInitialForTransitionCell(Config::GFX_SHADER_COMPILATION_MODE, self.m_mode_label);
  GSUSetInitialForBool(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, false, self.m_compile_shaders_switch, self.m_compile_shaders_label);
}

- (IBAction)VSyncChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_VSYNC, [self.m_vsync_switch isOn]);
}

- (IBAction)FPSChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_SHOW_FPS, [self.m_fps_switch isOn]);
}

- (IBAction)NetPlayMessagesChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_SHOW_NETPLAY_MESSAGES, [self.m_netplay_messages_switch isOn]);
}

- (IBAction)RenderTimeLogChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_LOG_RENDER_TIME_TO_FILE, [self.m_render_time_log_switch isOn]);
}

- (IBAction)NetPlayShowPingChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_SHOW_NETPLAY_PING, [self.m_netplay_ping_switch isOn]);
}

- (IBAction)CompileShadersChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, [self.m_compile_shaders_switch isOn]);
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  switch (indexPath.section)
  {
    case 0:
      switch (indexPath.row)
      {
        case 0:
        {
          // Terrible hack - DOLocalizedString gets called *twice* - once here and once in GSUShowHelpWithString!
          // This is done so that people don't see "select OpenGL" and use it, when Vulkan is the most performant.
          NSString* backend_message = DOLocalizedString(@"Selects which graphics API to use internally.\n\nThe software renderer is extremely "
                                                        "slow and only useful for debugging, so any of the other backends are "
                                                        "recommended.\n\nIf unsure, select OpenGL.");
          GSUShowHelpWithString([backend_message stringByReplacingOccurrencesOfString:@"OpenGL" withString:@"Vulkan"]);
          break;
        }
        case 1:
          GSUShowHelpWithString(@"Selects which aspect ratio to use when rendering.\n\nAuto: Uses the native aspect "
                                "ratio\nForce 16:9: Mimics an analog TV with a widescreen aspect ratio.\nForce 4:3: "
                                "Mimics a standard 4:3 analog TV.\nStretch to Window: Stretches the picture to the "
                                "window size.\n\nIf unsure, select Auto.");
          break;
        case 2:
          GSUShowHelpWithString(@"Waits for vertical blanks in order to prevent tearing.\n\nDecreases performance "
                                "if emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 1:
      switch (indexPath.row)
      {
        case 0:
          GSUShowHelpWithString(@"Shows the number of frames rendered per second as a measure of "
                                "emulation speed.\n\nIf unsure, leave this unchecked.");
          break;
        case 1:
          GSUShowHelpWithString(@"Shows chat messages, buffer changes, and desync alerts "
                                "while playing NetPlay.\n\nIf unsure, leave this unchecked.");
          break;
        case 2:
          GSUShowHelpWithString(@"Logs the render time of every frame to User/Logs/render_time.txt.\n\nUse this "
                                "feature when to measure the performance of Dolphin.\n\nIf "
                                "unsure, leave this unchecked.");
          break;
        case 3:
          GSUShowHelpWithString(@"Shows the player's maximum ping while playing on "
                                "NetPlay.\n\nIf unsure, leave this unchecked.");
          break;
      }
      break;
    case 2:
      GSUShowHelpWithString(@"Waits for all shaders to finish compiling before starting a game. Enabling this "
                            "option may reduce stuttering or hitching for a short time after the game is "
                            "started, at the cost of a longer delay before the game starts. For systems with "
                            "two or fewer cores, it is recommended to enable this option, as a large shader "
                            "queue may reduce frame rates.\n\nOtherwise, if unsure, leave this unchecked.");
      break;
  }
}

@end
