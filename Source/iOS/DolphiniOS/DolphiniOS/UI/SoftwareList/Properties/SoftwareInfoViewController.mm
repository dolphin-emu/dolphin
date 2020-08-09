// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareInfoViewController.h"

#import "DiscIO/Enums.h"

@interface SoftwareInfoViewController ()

@end

@implementation SoftwareInfoViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  NSString* game_name;
  std::string cpp_game_name = self.m_game_file->GetInternalName();
  if (cpp_game_name.empty())
  {
    game_name = DOLocalizedString(@"Unknown");
  }
  else
  {
    game_name = CppToFoundationString(cpp_game_name);
  }
  
  if (DiscIO::IsDisc(self.m_game_file->GetPlatform()))
  {
    NSString* base_str = DOLocalizedStringWithArgs(@"%1 (Disc %2, Revision %3)", @"@", @"d", @"d");
    [self.m_name_label setText:[NSString stringWithFormat:base_str, game_name, self.m_game_file->GetDiscNumber(), self.m_game_file->GetRevision()]];
  }
  else
  {
    NSString* base_str = DOLocalizedStringWithArgs(@"%1 (Revision %3)", @"@", @"d");
    [self.m_name_label setText:[NSString stringWithFormat:base_str, game_name, self.m_game_file->GetRevision()]];
  }
  
  NSString* game_id = CppToFoundationString(self.m_game_file->GetGameID());
  
  u64 title_id = self.m_game_file->GetTitleID();
  if (title_id)
  {
    game_id = [game_id stringByAppendingString:[NSString stringWithFormat:@" (%016llx)", title_id]];
  }
  
  [self.m_game_id_label setText:game_id];
  
  [self.m_country_label setText:CppToFoundationString(DiscIO::GetName(self.m_game_file->GetCountry(), true))];
  
  NSString* maker;
  std::string cpp_maker = self.m_game_file->GetMaker(UICommon::GameFile::Variant::LongAndNotCustom);
  if (cpp_maker.empty())
  {
    maker = DOLocalizedString(@"Unknown");
  }
  else
  {
    maker = CppToFoundationString(cpp_maker);
  }
  
  maker = [maker stringByAppendingString:[NSString stringWithFormat:@" (%@)", CppToFoundationString(self.m_game_file->GetMakerID())]];
  
  [self.m_maker_label setText:maker];
  
  [self.m_apploader_label setText:CppToFoundationString(self.m_game_file->GetApploaderDate())];
}

@end
