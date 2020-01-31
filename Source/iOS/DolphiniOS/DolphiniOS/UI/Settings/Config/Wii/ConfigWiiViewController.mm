// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigWiiViewController.h"

#import "Core/Config/SYSCONFSettings.h"
#import "Core/ConfigManager.h"
#import "Core/IOS/IOS.h"

@interface ConfigWiiViewController ()

@end

@implementation ConfigWiiViewController

// Skipped settings:
// Connect USB Keyboard - can't connect a USB keyboard as of now
// Whitelisted USB Passthrough Devices (all) - can't connect arbitrary USB devices to iOS

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_pal60_switch setOn:Config::Get(Config::SYSCONF_PAL60)];
  [self.m_screen_saver_switch setOn:Config::Get(Config::SYSCONF_SCREENSAVER)];
  [self.m_sd_card_switch setOn:SConfig::GetInstance().m_WiiSDCard];
  [self.m_ir_slider setValue:Config::Get(Config::SYSCONF_SENSOR_BAR_SENSITIVITY)];
  [self.m_volume_slider setValue:Config::Get(Config::SYSCONF_SPEAKER_VOLUME)];
  [self.m_rumble_switch setOn:Config::Get(Config::SYSCONF_WIIMOTE_MOTOR)];
}

- (IBAction)Pal60Changed:(id)sender
{
  Config::SetBase(Config::SYSCONF_PAL60, [self.m_pal60_switch isOn]);
}

- (IBAction)ScreenSaverChanged:(id)sender
{
  Config::SetBase(Config::SYSCONF_SCREENSAVER, [self.m_screen_saver_switch isOn]);
}

- (IBAction)SDCardChanged:(id)sender
{
  SConfig::GetInstance().m_WiiSDCard = [self.m_sd_card_switch isOn];
  
  auto* ios = IOS::HLE::GetIOS();
  if (ios)
  {
    ios->SDIO_EventNotify();
  }
}

- (IBAction)IRSensitivityChanged:(id)sender
{
  Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_SENSITIVITY, (u32)[self.m_ir_slider value]);
}

- (IBAction)SpeakerVolumeChanged:(id)sender
{
  Config::SetBase<u32>(Config::SYSCONF_SPEAKER_VOLUME, (u32)[self.m_volume_slider value]);
}

- (IBAction)RumbleChanged:(id)sender
{
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR, [self.m_rumble_switch isOn]);
}

@end
