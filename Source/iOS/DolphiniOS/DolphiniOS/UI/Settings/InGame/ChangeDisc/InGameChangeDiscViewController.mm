// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InGameChangeDiscViewController.h"

#import "Common/CDUtils.h"

#import "Core/Core.h"
#import "Core/HW/DVD/DVDInterface.h"

#import "GameFileCacheHolder.h"

#import "SoftwareTableViewCell.h"

#import "UICommon/GameFile.h"

@interface InGameChangeDiscViewController ()

@end

@implementation InGameChangeDiscViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.m_cache = [[GameFileCacheHolder sharedInstance] m_cache];
  [self rescan];
}

- (void)rescan
{
  NSMutableArray<NSNumber*>* indices = [[NSMutableArray alloc] init];
  
  for (size_t i = 0; i < self.m_cache->GetSize(); i++)
  {
    std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get(i);
    DiscIO::Platform platform = file->GetPlatform();
    
    if (platform == DiscIO::Platform::GameCubeDisc || platform == DiscIO::Platform::WiiDisc)
    {
      [indices addObject:[NSNumber numberWithUnsignedLong:i]];
    }
  }
  
  self.m_valid_indices = [indices copy];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return [self.m_valid_indices count];
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareTableViewCell* cell = (SoftwareTableViewCell*)[tableView dequeueReusableCellWithIdentifier:@"software_cell" forIndexPath:indexPath];
  
  // Get the GameFile
  NSNumber* number = [self.m_valid_indices objectAtIndex:indexPath.row];
  std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get([number unsignedLongValue]);
  DiscIO::Platform platform = file->GetPlatform();
  
  NSString* cell_contents = @"";
  
  // Add the platform prefix
  if (platform == DiscIO::Platform::GameCubeDisc)
  {
    cell_contents = [cell_contents stringByAppendingString:@"[GC] "];
  }
  else if (platform == DiscIO::Platform::WiiDisc)
  {
    cell_contents = [cell_contents stringByAppendingString:@"[Wii] "];
  }
  
  cell_contents = [cell_contents stringByAppendingString:CppToFoundationString(file->GetUniqueIdentifier())];
  
  // Set the cell label text
  cell.fileNameLabel.text = cell_contents;

  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSNumber* number = [self.m_valid_indices objectAtIndex:indexPath.row];
  std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get([number unsignedLongValue]);
  
  Core::RunAsCPUThread([file_path = file->GetFilePath()]
  {
    DVDInterface::ChangeDisc(file_path);
  });
  
  [self.navigationController dismissViewControllerAnimated:true completion:nil];
}

@end
