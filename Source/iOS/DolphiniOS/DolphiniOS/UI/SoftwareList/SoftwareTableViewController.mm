// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareTableViewController.h"

#import "DolphiniOS-Swift.h"
#import "EmulationViewController.h"
#import "SoftwareTableViewCell.h"

#import "MainiOS.h"

#import "Common/FileUtil.h"

#import "UICommon/GameFile.h"

@interface SoftwareTableViewController ()

@end

@implementation SoftwareTableViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Load the GameFileCache
  self.m_cache = new UICommon::GameFileCache();
  self.m_cache->Load();
  
  [self rescanGameFilesWithRefreshing:false];
  
  // Create a UIRefreshControl
  UIRefreshControl* refreshControl = [[UIRefreshControl alloc] init];
  [refreshControl addTarget:self action:@selector(refreshGameFileCache) forControlEvents:UIControlEventValueChanged];
  
  self.tableView.refreshControl = refreshControl;
}

- (void)refreshGameFileCache
{
  [self rescanGameFilesWithRefreshing:true];
}

- (void)rescanGameFilesWithRefreshing:(bool)refreshing
{
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
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
    cache_updated |= self.m_cache->UpdateAdditionalMetadata();
    if (cache_updated)
    {
      self.m_cache->Save();
    }
    
    self.m_cache_loaded = true;
    
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.tableView reloadData];
      
      if (refreshing)
      {
        [self.tableView.refreshControl endRefreshing];
      }
    });
  });
}

- (void)showAlertWithTitle:(NSString*)title text:(NSString*)text isFatal:(bool)isFatal
{
  UIAlertController* alert = [UIAlertController alertControllerWithTitle:title
                                 message:text
                                 preferredStyle:UIAlertControllerStyleAlert];
   
  UIAlertAction* okayAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
     handler:^(UIAlertAction * action) {
    if (isFatal)
    {
      exit(0);
    }
  }];
   
  [alert addAction:okayAction];
  [self.navigationController presentViewController:alert animated:YES completion:nil];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
}

- (IBAction)addButtonPressed:(id)sender
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

#pragma mark - Table view data source

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
  SoftwareTableViewCell* cell =
      (SoftwareTableViewCell*)[tableView dequeueReusableCellWithIdentifier:@"softwareCell"
                                                              forIndexPath:indexPath];
  
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
  
  // Append the game name
  NSString* game_name = [NSString stringWithUTF8String:file->GetLongName().c_str()];
  
  if ([game_name length] == 0)
  {
    game_name = [NSString stringWithUTF8String:file->GetFileName().c_str()];
  }
  
  cell_contents = [cell_contents stringByAppendingString:game_name];
  
  // Set the cell label text
  cell.fileNameLabel.text = cell_contents;

  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  [self performSegueWithIdentifier:@"toEmulation" sender:nil];
}

- (UISwipeActionsConfiguration*)tableView:(UITableView*)tableView trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
{
  UIContextualAction* delete_action = [UIContextualAction contextualActionWithStyle:UIContextualActionStyleDestructive title:DOLocalizedString(@"Delete") handler:^(UIContextualAction* action, __kindof UIView* source_view, void (^completion_handler)(bool)) {
    std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get(indexPath.row);
    if (File::Delete(file->GetFilePath()))
    {
      [self rescanGameFilesWithRefreshing:false];
    }
    
    completion_handler(false);
  }];

  UISwipeActionsConfiguration* actions = [UISwipeActionsConfiguration configurationWithActions:@[ delete_action ]];
  actions.performsFirstActionWithFullSwipe = false;
  
  return actions;
}

#pragma mark - Document picker delegate methods

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
  NSSet<NSURL*>* set = [NSSet setWithArray:urls];
  [MainiOS importFiles:set];
  
  [self rescanGameFilesWithRefreshing:false];
}

#pragma mark - Segues

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"toEmulation"])
  {
    UINavigationController* navigationController = (UINavigationController*)segue.destinationViewController;
    EmulationViewController* viewController = (EmulationViewController*)([navigationController.viewControllers firstObject]);
    
    // Get the GameFile and set values
    std::shared_ptr<const UICommon::GameFile> file = self.m_cache->Get([self.tableView indexPathForSelectedRow].item);
    viewController.m_game_file = const_cast<UICommon::GameFile*>(file.get());
  }
}

- (IBAction)unwindToSoftwareTable:(UIStoryboardSegue*)segue {}

@end
