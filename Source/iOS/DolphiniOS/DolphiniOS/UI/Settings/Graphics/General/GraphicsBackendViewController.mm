// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsBackendViewController.h"

#import "Common/Config/Config.h"

#import "Core/Config/MainSettings.h"

#import "VideoCommon/VideoBackendBase.h"

@interface GraphicsBackendViewController ()

@end

@implementation GraphicsBackendViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self->m_backends.push_back("OGL");
  self->m_backends.push_back("Vulkan");
  self->m_backends.push_back("Software Renderer");
  self->m_backends.push_back("Null");
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  for (size_t i = 0; i < self->m_backends.size(); i++)
  {
    if (self->m_backends[i] == Config::Get(Config::MAIN_GFX_BACKEND))
    {
      self.m_last_selected = i;
    }
  }
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  std::string backend = self->m_backends[indexPath.row];
  if (backend != Config::Get(Config::MAIN_GFX_BACKEND))
  {
    void (^ChangeBackend)() = ^{
      UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
      [old_cell setAccessoryType:UITableViewCellAccessoryNone];
      
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:indexPath.row inSection:0]];
      [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
      
      Config::SetBase(Config::MAIN_GFX_BACKEND, backend);
      VideoBackendBase::PopulateBackendInfoFromUI();
      
      self.m_last_selected = indexPath.row;
    };
    
    auto warning_message = g_available_video_backends[indexPath.row]->GetWarningMessage();
    
    if (warning_message)
    {
      NSString* warning_message_ns = [NSString stringWithUTF8String:warning_message->c_str()];
      UIAlertController* alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Confirm backend change") message:warning_message_ns preferredStyle:UIAlertControllerStyleAlert];
      
      [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"No") style:UIAlertActionStyleDefault handler:nil]];
      
      [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Yes") style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action) {
        ChangeBackend();
      }]];
      
      [self presentViewController:alert animated:true completion:nil];
    }
    else
    {
      ChangeBackend();
    }
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
