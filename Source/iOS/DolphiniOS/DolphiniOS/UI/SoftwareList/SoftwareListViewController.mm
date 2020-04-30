// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareListViewController.h"

#import "Common/CommonPaths.h"
#import "Common/FileUtil.h"

#import "MainiOS.h"

#import "SoftwareCollectionViewCell.h"
#import "SoftwareTableViewCell.h"

#import "UICommon/GameFile.h"

@interface SoftwareListViewController ()

@end

@implementation SoftwareListViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Load the GameFileCache
  self.m_cache = new UICommon::GameFileCache();
  self.m_cache->Load();
  
  [self rescanWithCompletionHandler:^{
    [self.m_table_view reloadData];
    [self.m_collection_view reloadData];
  }];
}

- (void)rescanWithCompletionHandler:(void (^)())completionHandler
{
  // Get the software folder path
  NSString* userDirectory = [MainiOS getUserFolder];
  NSString* softwareDirectory = [userDirectory stringByAppendingPathComponent:@"Software"];

  // Create it if necessary
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:softwareDirectory])
  {
    [fileManager createDirectoryAtPath:softwareDirectory withIntermediateDirectories:false
                 attributes:nil error:nil];
  }
  
  std::vector<std::string> folder_paths;
  folder_paths.push_back(std::string([softwareDirectory UTF8String]));
  
  // Update the cache
  bool cache_updated = self.m_cache->Update(UICommon::FindAllGamePaths(folder_paths, false));
  if (cache_updated)
  {
    self.m_cache->Save();
  }
  
  self.m_cache_loaded = true;
  
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    self.m_cache->UpdateAdditionalMetadata();
    
    dispatch_async(dispatch_get_main_queue(), ^{
      completionHandler();
    });
  });
}

- (NSString*)GetGameName:(std::shared_ptr<const UICommon::GameFile>)file
{
  // Get the long name
  NSString* game_name = CppToFoundationString(file->GetLongName());
  
  // Use the file name as a fallback
  if ([game_name length] == 0)
  {
    return CppToFoundationString(file->GetFileName());
  }
  
  return game_name;
}

#pragma mark - Collection View

- (NSInteger)collectionView:(UICollectionView*)collectionView numberOfItemsInSection:(NSInteger)section
{
  if (!self.m_cache_loaded)
  {
    return 0;
  }
  
  return self.m_cache->GetSize();
}

- (UICollectionViewCell *)collectionView:(UICollectionView*)collectionView cellForItemAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareCollectionViewCell* cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"software_view_cell" forIndexPath:indexPath];
  
  std::shared_ptr<const UICommon::GameFile> game_file = self.m_cache->Get(indexPath.row);
  
  [cell.m_name_label setText:[self GetGameName:game_file]];
  
  const std::string cover_path = File::GetUserPath(D_COVERCACHE_IDX) + DIR_SEP;
  const std::string png_path = cover_path + game_file->GetGameTDBID() + ".png";
  
  UIImage* image;
  if (File::Exists(png_path))
  {
    image = [UIImage imageWithContentsOfFile:CppToFoundationString(png_path)];
  }
  else
  {
    image = [UIImage imageNamed:@"ic_launcher"];
  }
  
  [cell.m_image_view setImage:image];
  
  return cell;
}

#pragma mark - Table View

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  if (!self.m_cache_loaded)
  {
    return 0;
  }
  
  return self.m_cache->GetSize();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareTableViewCell* cell = (SoftwareTableViewCell*)[tableView dequeueReusableCellWithIdentifier:@"softwareCell" forIndexPath:indexPath];
  
  NSString* cell_contents = @"";
  
  // Get the GameFile
  std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get(indexPath.item);
  DiscIO::Platform platform = file->GetPlatform();
  
  // Add the platform prefix
  if (platform == DiscIO::Platform::GameCubeDisc)
  {
    cell_contents = [cell_contents stringByAppendingString:@"[GC] "];
  }
  else if (platform == DiscIO::Platform::WiiDisc || platform == DiscIO::Platform::WiiWAD)
  {
    cell_contents = [cell_contents stringByAppendingString:@"[Wii] "];
  }
  else
  {
    cell_contents = [cell_contents stringByAppendingString:@"[Unk] "];
  }
  
  cell_contents = [cell_contents stringByAppendingString:[self GetGameName:file]];
  
  // Set the cell label text
  cell.fileNameLabel.text = cell_contents;

  return cell;
}

#pragma mark - Swap button

- (IBAction)SwapPressed:(id)sender
{
  [UIView transitionWithView:self.view duration:0.5 options:UIViewAnimationOptionTransitionFlipFromRight animations:^{
    [self.m_collection_view setHidden:![self.m_collection_view isHidden]];
    [self.m_table_view setHidden:![self.m_table_view isHidden]];
  } completion:nil];
}

@end
