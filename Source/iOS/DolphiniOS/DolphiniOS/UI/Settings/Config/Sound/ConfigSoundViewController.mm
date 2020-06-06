// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigSoundViewController.h"

#import "AudioCommon/AudioCommon.h"

#import "Core/ConfigManager.h"

@interface ConfigSoundViewController ()

@end

@implementation ConfigSoundViewController

// Skipped settings:
// DSP Emulation Engine - DSP LLE is unsupported on iOS; JIT is for x86_64
// Backend Settings (all) - CoreAudio is the only supported backend

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  //[self.m_volume_slider setValue:SConfig::GetInstance().m_Volume];
  //[self UpdateVolumePercentageLabel];
  [self.m_stretching_switch setOn:SConfig::GetInstance().m_audio_stretch];
  [self.m_buffer_size_slider setEnabled:SConfig::GetInstance().m_audio_stretch];
  [self.m_buffer_size_slider setValue:SConfig::GetInstance().m_audio_stretch_max_latency];
  [self UpdateBufferSizeLabel];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  AudioCommon::UpdateSoundStream();
}

- (IBAction)VolumeChanged:(id)sender
{
  SConfig::GetInstance().m_Volume = [self.m_volume_slider value];
  [self UpdateVolumePercentageLabel];
}

- (IBAction)StretchingToggled:(id)sender
{
  SConfig::GetInstance().m_audio_stretch = [self.m_stretching_switch isOn];
  [self.m_buffer_size_slider setEnabled:[self.m_stretching_switch isOn]];
}

- (IBAction)BufferSizeChanged:(id)sender
{
  SConfig::GetInstance().m_audio_stretch_max_latency = [self.m_buffer_size_slider value];
  [self UpdateBufferSizeLabel];
}

- (void)UpdateVolumePercentageLabel
{
  [self.m_volume_perecentage_label setText:[NSString stringWithFormat:@"%d%%", SConfig::GetInstance().m_Volume]];
}

- (void)UpdateBufferSizeLabel
{
  [self.m_buffer_size_label setText:[NSString stringWithFormat:DOLocalizedStringWithArgs(@"%1 ms", @"d"), SConfig::GetInstance().m_audio_stretch_max_latency]];
}

@end
