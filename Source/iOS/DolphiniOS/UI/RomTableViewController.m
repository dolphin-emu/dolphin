//
//  iNDSMasterViewController.m
//  iNDS
//
//  Created by iNDS on 6/9/13.
//  Copyright (c) 2014 iNDS. All rights reserved.
//

#import "AppDelegate.h"

#import "MHWDirectoryWatcher.h"
#import "WCEasySettingsViewController.h"
#import "SharkfoodMuteSwitchDetector.h"
#import "SCLAlertView.h"

#import "UI/RomTableViewController.h"
#import "UI/GameTableView.h"

#import "Bridge/DolphinGame.h"

#import <mach/mach.h>
#import <mach/mach_host.h>

@interface RomTableViewController () {
    NSMutableArray * activeDownloads;
    MHWDirectoryWatcher * docWatchHelper;
}

@end

@implementation RomTableViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    [[UIApplication sharedApplication] setStatusBarStyle:UIStatusBarStyleLightContent animated:NO];
    self.navigationItem.title = @"Roms";
    [SharkfoodMuteSwitchDetector shared]; //Start detecting
    
    
    mach_port_t host_port;
    mach_msg_type_number_t host_size;
    vm_size_t pagesize;
    
    host_port = mach_host_self();
    host_size = sizeof(vm_statistics_data_t) / sizeof(integer_t);
    host_page_size(host_port, &pagesize);
    
    printf("Page size: %lu", pagesize);
    
    vm_statistics_data_t vm_stat;
    
    if (host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &host_size) != KERN_SUCCESS) {
        NSLog(@"Failed to fetch vm statistics");
    }
    
    /* Stats in bytes */
    natural_t mem_used = (vm_stat.active_count +
                          vm_stat.inactive_count +
                          vm_stat.wire_count) * pagesize;
    natural_t mem_free = vm_stat.free_count * pagesize;
    natural_t mem_total = mem_used + mem_free;
    
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [[UIApplication sharedApplication] setStatusBarStyle:UIStatusBarStyleLightContent animated:NO];
    [self reloadGames:nil];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    // watch for changes in documents folder
    docWatchHelper = [MHWDirectoryWatcher directoryWatcherAtPath:AppDelegate.sharedInstance.documentsPath
                                                        callback:^{
                                                            [self reloadGames:self];
                                                        }];
}


- (IBAction)openSettings:(id)sender
{
    WCEasySettingsViewController *settingsView = AppDelegate.sharedInstance.settingsViewController;
    settingsView.title = @"Emulator Settings";
    
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:settingsView];
    UIColor *globalTint = [[[UIApplication sharedApplication] delegate] window].tintColor;
    
    nav.navigationBar.barTintColor = globalTint;
    nav.navigationBar.translucent = NO;
    nav.navigationBar.tintColor = [UIColor whiteColor];
    [nav.navigationBar setTitleTextAttributes:@{NSForegroundColorAttributeName : [UIColor whiteColor]}];
    
    [self presentViewController:nav animated:YES completion:nil];
}

- (void)reloadGames:(id)sender
{
    NSLog(@"Reloading");
    if (sender == self) {
        // do it later, the file may not be written yet
        [self performSelector:_cmd withObject:nil afterDelay:1];
    } else  {
        // reload all games
        games = [DolphinGame gamesAtPath:AppDelegate.sharedInstance.documentsPath saveStateDirectoryPath:AppDelegate.sharedInstance.batteryDir];
        NSString *romsPath = [[NSBundle mainBundle] resourcePath];
        games = [games arrayByAddingObjectsFromArray:[DolphinGame gamesAtPath:romsPath saveStateDirectoryPath:AppDelegate.sharedInstance.batteryDir]];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.tableView reloadData];
        });
    }
}


#pragma mark - Table View

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
    return YES;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        if (indexPath.section == 0) {  // Del game
            DolphinGame *game = games[indexPath.row];
            if ([[NSFileManager defaultManager] removeItemAtPath:game.path error:NULL]) {
                [self reloadGames:nil];
                [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
            }
        } else {
            if (indexPath.row >= activeDownloads.count) return;
            [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
        }
    }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (section == 0)
        return games.count;
    return activeDownloads.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 2;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell;
    if (indexPath.section == 0) { // Game
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"DolphinGame"];
        if (indexPath.row >= games.count) return [UITableViewCell new];
        DolphinGame *game = games[indexPath.row];
        if (game.gameTitle) {
            // use title from ROM
            NSArray *titleLines = [game.gameTitle componentsSeparatedByString:@"\n"];
            cell.textLabel.text = titleLines[0];
            cell.detailTextLabel.text = titleLines.count >= 1 ? titleLines[1] : nil;
        } else {
            // use filename
            cell.textLabel.text = game.title;
            cell.detailTextLabel.text = nil;
        }
        cell.imageView.image = game.icon;
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    } else { //Download
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"iNDSDownload"];
        if (indexPath.row >= activeDownloads.count) return [UITableViewCell new];
        cell.detailTextLabel.text = @"Waiting...";
        cell.imageView.image = nil;
    }
    return cell;
}

#pragma mark - Select ROMs

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section == 0) {
        DolphinGame *game = games[indexPath.row];
        GameTableView * gameInfo = [[GameTableView alloc] init];
        gameInfo.game = game;
        [self.navigationController pushViewController:gameInfo animated:YES];
        [tableView deselectRowAtIndexPath:indexPath animated:YES];
    } else {
        [tableView deselectRowAtIndexPath:indexPath animated:YES];
    }
}


#pragma mark - Debug


@end

