// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareListViewController.h"

#import "Common/CDUtils.h"
#import "Common/CommonPaths.h"
#import "Common/FileUtil.h"

#import "Core/CommonTitles.h"
#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"
#import "Core/IOS/ES/ES.h"
#import "Core/IOS/IOS.h"
#import "Core/WiiUtils.h"

#import "EmulationViewController.h"

#import "GameFileCacheHolder.h"

#import "MainiOS.h"

#import "SoftwareCollectionViewCell.h"
#import "SoftwarePropertiesViewController.h"
#import "SoftwareTableViewCell.h"

#import "UICollectionViewLeftAlignedLayout.h"

#import "UICommon/GameFile.h"

#import "WiiSystemUpdateViewController.h"

@interface SoftwareListViewController ()

@end

@implementation SoftwareListViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  NSInteger preferred_view = [[NSUserDefaults standardUserDefaults] integerForKey:@"software_list_view"];
  
  [self.m_table_view setHidden:preferred_view == 0];
  [self.m_collection_view setHidden:preferred_view == 1];
  
  self.m_navigation_item.rightBarButtonItems = @[
    self.m_add_button,
    preferred_view == 0 ? self.m_list_button : self.m_grid_button
  ];
  
  // Assign images to navigation buttons using SF symbols when supported
  if (@available(iOS 13, *))
  {
    self.m_menu_button.image = [UIImage imageNamed:@"SF_ellipsis_circle"];
    self.m_grid_button.image = [UIImage imageNamed:@"SF_rectangle_grid_2x2"];
    self.m_list_button.image = [UIImage imageNamed:@"SF_list_dash"];
  }
  else
  {
    self.m_menu_button.image = [UIImage imageNamed:@"ellipsis_circle_legacy.png"];
    self.m_grid_button.image = [UIImage imageNamed:@"rectangle_grid_2x2_legacy.png"];
    self.m_list_button.image = [UIImage imageNamed:@"list_dash_legacy.png"];
  }
  
  // Create UIRefreshControls
  UIRefreshControl* table_refresh = [[UIRefreshControl alloc] init];
  [table_refresh addTarget:self action:@selector(handleRefresh:) forControlEvents:UIControlEventValueChanged];
  self.m_table_view.refreshControl = table_refresh;
  
  UIRefreshControl* collection_refresh = [[UIRefreshControl alloc] init];
  [collection_refresh addTarget:self action:@selector(handleRefresh:) forControlEvents:UIControlEventValueChanged];
  self.m_collection_view.refreshControl = collection_refresh;
  
  // Left align on devices that aren't compact horizontally
  /*if (self.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular)
  {
    UICollectionViewFlowLayout* layout = [[UICollectionViewLeftAlignedLayout alloc] init];
    layout.estimatedItemSize = UICollectionViewFlowLayoutAutomaticSize;
    self.m_collection_view.collectionViewLayout = layout;
  }*/
  
  // Load the GameFileCache
  self.m_cache = [[GameFileCacheHolder sharedInstance] m_cache];
  self.m_cache_loaded = true;
  
  [[GameFileCacheHolder sharedInstance] fetchMetadataWithCompletionHandler:^{
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.m_table_view reloadData];
      [self.m_collection_view reloadData];
    });
  }];
}

- (void)rescanWithCompletionHandler:(void (^)())completionHandler
{
  if ([[GameFileCacheHolder sharedInstance] m_is_busy])
  {
    if (completionHandler)
    {
      completionHandler();
    }
  }
  
  self.m_cache_loaded = false;
  
  [[GameFileCacheHolder sharedInstance] completeRescanWithCompletionHandler:^{
    self.m_cache_loaded = true;
    
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.m_table_view reloadData];
      [self.m_collection_view reloadData];
      
      if (completionHandler)
      {
        completionHandler();
      }
    });
  }];
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

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView*)collectionView
{
  return 1;
}

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
  
  // Weird issue - the UICollectionView will attempt to load the cells
  // before updating the number of items. Then, it will update the
  // number of items and load the cells again. No idea what's going on
  // here, but let's just make a dummy cell if this cell is for a now
  // deleted GameFile.
  if (self.m_cache->GetSize() <= indexPath.row)
  {
    [cell.m_name_label setText:@"ERROR"];
    [cell.m_image_view setImage:[UIImage imageNamed:@"ic_launcher"]];
    return cell;
  }
  
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

- (void)collectionView:(UICollectionView*)collectionView didSelectItemAtIndexPath:(NSIndexPath*)indexPath
{
  self.m_selected_file = self.m_cache->Get(indexPath.row).get();
  self.m_boot_type = DOLBootTypeGame;
  
  [self performSegueWithIdentifier:@"to_emulation" sender:nil];
  
  [self.m_collection_view deselectItemAtIndexPath:indexPath animated:true];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath point:(CGPoint)point API_AVAILABLE(ios(13.0))
{
  return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:^(NSArray<UIMenuElement*>*)
  {
    return [self CreateContextMenuAtIndexPath:indexPath];
  }];
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
    cell_contents = [cell_contents stringByAppendingString:@"[Homebrew] "];
  }
  
  cell_contents = [cell_contents stringByAppendingString:[self GetGameName:file]];
  
  // Set the cell label text
  cell.fileNameLabel.text = cell_contents;

  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  self.m_selected_file = self.m_cache->Get(indexPath.row).get();
  
  self.m_boot_type = DOLBootTypeGame;
  [self performSegueWithIdentifier:@"to_emulation" sender:nil];
  
  [self.m_table_view deselectRowAtIndexPath:indexPath animated:true];
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath point:(CGPoint)point API_AVAILABLE(ios(13.0))
{
  return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:^(NSArray<UIMenuElement*>*)
  {
    return [self CreateContextMenuAtIndexPath:indexPath];
  }];
}

#pragma mark - Context menu

- (UIMenu*)CreateContextMenuAtIndexPath:(NSIndexPath*)indexPath API_AVAILABLE(ios(13.0))
{
  const UICommon::GameFile* game_file = self.m_cache->Get(indexPath.row).get();
  
  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];
  
  [actions addObject:[UIAction actionWithTitle:DOLocalizedString(@"Properties") image:[UIImage systemImageNamed:@"square.and.pencil"] identifier:nil handler:^(UIAction*)
  {
    [self OpenProperties:game_file];
  }]];
  
  DiscIO::Platform platform = game_file->GetPlatform();
  if (platform == DiscIO::Platform::WiiDisc || platform == DiscIO::Platform::GameCubeDisc)
  {
    [actions addObject:[UIAction actionWithTitle:DOLocalizedString(@"Set as Default ISO") image:[UIImage systemImageNamed:@"doc"] identifier:nil handler:^(UIAction*)
    {
      [self SetDefaultIso:game_file];
    }]];
  }
  else if (platform == DiscIO::Platform::WiiWAD)
  {
    if (!WiiUtils::IsTitleInstalled(game_file->GetTitleID()))
    {
      [actions addObject:[UIAction actionWithTitle:DOLocalizedString(@"Install to the NAND") image:[UIImage systemImageNamed:@"rectangle.stack.badge.plus"] identifier:nil handler:^(UIAction*)
      {
        [self InstallWad:game_file];
      }]];
    }
    else
    {
      [actions addObject:[UIAction actionWithTitle:DOLocalizedString(@"Uninstall from the NAND") image:[UIImage systemImageNamed:@"rectangle.stack.badge.minus"] identifier:nil handler:^(UIAction*)
      {
        [self UninstallWad:game_file];
      }]];
    }
  }
  
  UIAction* delete_action = [UIAction actionWithTitle:DOLocalizedString(@"Delete") image:[UIImage systemImageNamed:@"trash"] identifier:nil handler:^(UIAction*)
  {
    [self DeleteFile:game_file];
  }];
  [delete_action setAttributes:UIMenuElementAttributesDestructive];
  
  [actions addObject:delete_action];
  
  return [UIMenu menuWithTitle:CppToFoundationString(game_file->GetUniqueIdentifier()) children:[actions copy]];
}

#pragma mark - Long press

- (IBAction)HandleLongPress:(UILongPressGestureRecognizer*)recognizer
{
  if (@available(iOS 13, *))
  {
    return;
  }
  
  if (recognizer.state != UIGestureRecognizerStateBegan)
  {
    return;
  }
  
  bool table_view = ![self.m_table_view isHidden];
  CGPoint point = table_view ? [recognizer locationInView:self.m_table_view] : [recognizer locationInView:self.m_collection_view];
  NSIndexPath* index_path = table_view ? [self.m_table_view indexPathForRowAtPoint:point] : [self.m_collection_view indexPathForItemAtPoint:point];
  UIView* source_view = table_view ? [self.m_table_view cellForRowAtIndexPath:index_path] : [self.m_collection_view cellForItemAtIndexPath:index_path];
  
  const UICommon::GameFile* game_file = self.m_cache->Get(index_path.row).get();
  
  UIAlertController* action_sheet = [UIAlertController alertControllerWithTitle:nil message:CppToFoundationString(game_file->GetUniqueIdentifier()) preferredStyle:UIAlertControllerStyleActionSheet];
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Properties") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
  {
    [self OpenProperties:game_file];
  }]];
  
  DiscIO::Platform platform = game_file->GetPlatform();
  if (platform == DiscIO::Platform::WiiDisc || platform == DiscIO::Platform::GameCubeDisc)
  {
    [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Set as Default ISO") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
    {
      [self SetDefaultIso:game_file];
    }]];
  }
  else if (platform == DiscIO::Platform::WiiWAD)
  {
    if (!WiiUtils::IsTitleInstalled(game_file->GetTitleID()))
    {
      [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Install to the NAND") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        [self InstallWad:game_file];
      }]];
    }
    else
    {
      [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Uninstall from the NAND") style:UIAlertActionStyleDestructive handler:^(UIAlertAction*)
      {
        [self UninstallWad:game_file];
      }]];
    }
  }
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Delete") style:UIAlertActionStyleDestructive handler:^(UIAlertAction*)
  {
    [self DeleteFile:game_file];
  }]];
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Cancel") style:UIAlertActionStyleCancel handler:nil]];
  
  if (action_sheet.popoverPresentationController != nil)
  {
    action_sheet.popoverPresentationController.sourceView = source_view;
    action_sheet.popoverPresentationController.sourceRect = source_view.bounds;
  }
  
  [self presentViewController:action_sheet animated:true completion:nil];
  
}

#pragma mark - Menu actions

- (void)OpenProperties:(const UICommon::GameFile*)game_file
{
  self.m_selected_file = game_file;
  [self performSegueWithIdentifier:@"to_properties" sender:nil];
}

- (void)SetDefaultIso:(const UICommon::GameFile*)game_file
{
  Config::SetBase(Config::MAIN_DEFAULT_ISO, game_file->GetFilePath());
}

- (void)DeleteFile:(const UICommon::GameFile*)game_file
{
  UIAlertController* confirm_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Confirm") message:DOLocalizedString(@"Are you sure you want to delete this file?") preferredStyle:UIAlertControllerStyleAlert];
  
  [confirm_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"No") style:UIAlertActionStyleDefault handler:nil]];
  
  [confirm_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Yes") style:UIAlertActionStyleDestructive handler:^(UIAlertAction*)
  {
    if (File::Delete(game_file->GetFilePath()))
    {
      [self rescanWithCompletionHandler:nil];
    }
  }]];
  
  [self presentViewController:confirm_alert animated:true completion:nil];
}

- (void)InstallWad:(const UICommon::GameFile*)game_file
{
  bool success = WiiUtils::InstallWAD(game_file->GetFilePath());
  
  UIAlertController* result_alert;
  if (success)
  {
    result_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Success") message:DOLocalizedString(@"Successfully installed this title to the NAND.") preferredStyle:UIAlertControllerStyleAlert];
  }
  else
  {
    result_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Failure") message:DOLocalizedString(@"Failed to install this title to the NAND.") preferredStyle:UIAlertControllerStyleAlert];
  }
  
  [result_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
  
  [self presentViewController:result_alert animated:true completion:nil];
}

- (void)UninstallWad:(const UICommon::GameFile*)game_file
{
  UIAlertController* confirm_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Confirm") message:DOLocalizedString(@"Uninstalling the WAD will remove the currently installed version of this title from the NAND without deleting its save data. Continue?") preferredStyle:UIAlertControllerStyleAlert];
  
  [confirm_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"No") style:UIAlertActionStyleDefault handler:nil]];
  
  [confirm_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Yes") style:UIAlertActionStyleDestructive handler:^(UIAlertAction*)
  {
    bool success = WiiUtils::UninstallTitle(game_file->GetTitleID());
    
    UIAlertController* result_alert;
    if (success)
    {
      result_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Success") message:DOLocalizedString(@"Successfully removed this title from the NAND.") preferredStyle:UIAlertControllerStyleAlert];
    }
    else
    {
      result_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Failure") message:DOLocalizedString(@"Failed to remove this title from the NAND.") preferredStyle:UIAlertControllerStyleAlert];
    }
    
    [result_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:result_alert animated:true completion:nil];
  }]];
  
  [self presentViewController:confirm_alert animated:true completion:nil];
}

#pragma mark - Swap button

- (IBAction)ChangeViewButtonPressed:(id)sender
{
  self.m_navigation_item.rightBarButtonItems = @[
    self.m_add_button,
    [self.m_table_view isHidden] ? self.m_grid_button : self.m_list_button
  ];
  
  NSInteger new_view = [self.m_table_view isHidden] ? 1 : 0;
  [[NSUserDefaults standardUserDefaults] setInteger:new_view forKey:@"software_list_view"];
  
  [UIView transitionWithView:self.view duration:0.5 options:UIViewAnimationOptionTransitionFlipFromRight animations:^{
    [self.m_collection_view setHidden:![self.m_collection_view isHidden]];
    [self.m_table_view setHidden:![self.m_table_view isHidden]];
  } completion:nil];
}

#pragma mark - Add button

- (IBAction)AddPressed:(id)sender
{
  NSArray* types = @[
    @"org.dolphin-emu.ios.generic-software",
    @"org.dolphin-emu.ios.gamecube-software",
    @"org.dolphin-emu.ios.wii-software"
  ];
  
  UIDocumentPickerViewController* pickerController = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:types inMode:UIDocumentPickerModeOpen];
  pickerController.delegate = self;
  pickerController.modalPresentationStyle = UIModalPresentationPageSheet;
  
  [self presentViewController:pickerController animated:true completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
  NSSet<NSURL*>* set = [NSSet setWithArray:urls];
  [MainiOS importFiles:set];
  
  [self rescanWithCompletionHandler:nil];
}

#pragma mark - Refreshing

- (void)handleRefresh:(UIRefreshControl*)control
{
  [self rescanWithCompletionHandler:^{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      [control endRefreshing];
    });
  }];
}

#pragma mark - Menu

- (IBAction)MenuButtonPressed:(id)sender
{
  // Get the system menu TMD
  IOS::HLE::Kernel ios;
  const auto tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);
  
  UIAlertController* action_sheet = [UIAlertController alertControllerWithTitle:nil message:nil preferredStyle:UIAlertControllerStyleActionSheet];
  
  if (tmd.IsValid())
  {
    std::string system_region = DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion());
    NSString* base_title = DOLocalizedStringWithArgs(@"Load Wii System Menu %1", @"@");
    
    [action_sheet addAction:[UIAlertAction actionWithTitle:[NSString stringWithFormat:base_title, CppToFoundationString(system_region)] style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
    {
      self.m_boot_type = DOLBootTypeWiiMenu;
      [self performSegueWithIdentifier:@"to_emulation" sender:nil];
    }]];
  }
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Load GameCube Main Menu") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
  {
    self.m_boot_type = DOLBootTypeGCIPL;
    
    UIAlertController* region_sheet = [UIAlertController alertControllerWithTitle:nil message:DOLocalizedString(@"Region") preferredStyle:UIAlertControllerStyleActionSheet];
    
    if (File::Exists(SConfig::GetInstance().GetBootROMPath(JAP_DIR)))
    {
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"NTSC-J") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        self.m_ipl_region = DiscIO::Region::NTSC_J;
        [self performSegueWithIdentifier:@"to_emulation" sender:nil];
      }]];
    }
    
    if (File::Exists(SConfig::GetInstance().GetBootROMPath(USA_DIR)))
    {
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"NTSC-U") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        self.m_ipl_region = DiscIO::Region::NTSC_U;
        [self performSegueWithIdentifier:@"to_emulation" sender:nil];
      }]];
    }
    
    if (File::Exists(SConfig::GetInstance().GetBootROMPath(EUR_DIR)))
    {
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"PAL") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        self.m_ipl_region = DiscIO::Region::PAL;
        [self performSegueWithIdentifier:@"to_emulation" sender:nil];
      }]];
    }
    
    [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Cancel") style:UIAlertActionStyleCancel handler:nil]];
    
    if (region_sheet.popoverPresentationController != nil)
    {
      region_sheet.popoverPresentationController.barButtonItem = self.m_menu_button;
    }
    
    if ([[region_sheet actions] count] == 1)
    {
      UIAlertController* error_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Not Found") message:@"Could not find any GameCube Main Menu files. Please dump it from a GameCube and copy it into User/GC/Region as \"IPL.bin\"." preferredStyle:UIAlertControllerStyleAlert];
      
      [error_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
      
      [self presentViewController:error_alert animated:true completion:nil];
    }
    else
    {
      [self presentViewController:region_sheet animated:true completion:nil];
    }
  }]];
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Perform Online System Update") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
  {
    void (^do_system_update)(DiscIO::Region) = ^(DiscIO::Region region)
    {
      self.m_ipl_region = region;
      [self performSegueWithIdentifier:@"to_wii_update" sender:nil];
    };
    
    if (!tmd.IsValid())
    {
      UIAlertController* region_sheet = [UIAlertController alertControllerWithTitle:nil message:DOLocalizedString(@"Region") preferredStyle:UIAlertControllerStyleActionSheet];
      
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Europe") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        do_system_update(DiscIO::Region::PAL);
      }]];
      
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Japan") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        do_system_update(DiscIO::Region::NTSC_J);
      }]];
      
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Korea") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        do_system_update(DiscIO::Region::NTSC_K);
      }]];
    
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"United States") style:UIAlertActionStyleDefault handler:^(UIAlertAction*)
      {
        do_system_update(DiscIO::Region::NTSC_U);
      }]];
           
      [region_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Cancel") style:UIAlertActionStyleCancel handler:nil]];
      
      if (region_sheet.popoverPresentationController != nil)
      {
        region_sheet.popoverPresentationController.barButtonItem = self.m_menu_button;
      }
      
      [self presentViewController:region_sheet animated:true completion:nil];
    }
    else
    {
      do_system_update(DiscIO::Region::Unknown);
    }
  }]];
  
  [action_sheet addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Close") style:UIAlertActionStyleCancel handler:nil]];
  
  if (action_sheet.popoverPresentationController != nil)
  {
    action_sheet.popoverPresentationController.barButtonItem = self.m_menu_button;
  }
  
  [self presentViewController:action_sheet animated:true completion:nil];
}


#pragma mark - Segues

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_emulation"])
  {
    // Set the GameFile
    UINavigationController* navigationController = (UINavigationController*)segue.destinationViewController;
    EmulationViewController* viewController = (EmulationViewController*)([navigationController.viewControllers firstObject]);
    
    if (self.m_boot_type == DOLBootTypeWiiMenu)
    {
      viewController->m_boot_parameters = std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU});
    }
    else if (self.m_boot_type == DOLBootTypeGCIPL)
    {
      viewController->m_boot_parameters = std::make_unique<BootParameters>(BootParameters::IPL{self.m_ipl_region});
    }
    else
    {
      viewController->m_boot_parameters = BootParameters::GenerateFromFile(self.m_selected_file->GetFilePath());
    }
  }
  else if ([segue.identifier isEqualToString:@"to_wii_update"])
  {
    NSString* wii_update_region;
    switch (self.m_ipl_region)
    {
      case DiscIO::Region::NTSC_J:
        wii_update_region = @"JPN";
        break;
      case DiscIO::Region::NTSC_U:
        wii_update_region = @"USA";
        break;
      case DiscIO::Region::PAL:
        wii_update_region = @"EUR";
        break;
      case DiscIO::Region::NTSC_K:
        wii_update_region = @"KOR";
        break;
      case DiscIO::Region::Unknown:
      default:
        wii_update_region = @"";
        break;
    }
    
    WiiSystemUpdateViewController* update_controller = (WiiSystemUpdateViewController*)segue.destinationViewController;
    update_controller.m_is_online = true;
    update_controller.m_source = wii_update_region;
  }
  else if ([segue.identifier isEqualToString:@"to_properties"])
  {
    UINavigationController* navigationController = (UINavigationController*)segue.destinationViewController;
    SoftwarePropertiesViewController* properties_controller = (SoftwarePropertiesViewController*)([navigationController.viewControllers firstObject]);
    properties_controller.m_game_file = const_cast<UICommon::GameFile*>(self.m_selected_file);
  }
}

- (IBAction)unwindToSoftwareTable:(UIStoryboardSegue*)segue {}

@end
