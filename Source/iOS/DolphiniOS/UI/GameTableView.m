//
//  iNDSGameTableView.m
//  iNDS
//
//  Created by Will Cobb on 11/4/15.
//  Copyright © 2015 iNDS. All rights reserved.
//


#import "SCLAlertView.h"

#import "UI/GameTableView.h"
#import "UI/EmulatorViewController.h"

#import "Bridge/DolphinGame.h"

@interface GameTableView() {
    
}
@end

@implementation GameTableView

-(void) viewDidLoad
{
    [super viewDidLoad];
    self.navigationItem.title = _game.gameTitle;
}

-(void) viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self.tableView reloadData];
    
}

#pragma mark - Table View

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
    return indexPath.section != 0;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (section == 0)
        return 2;
    return _game.saveStates.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 2;
}

-(NSArray *)tableView:(UITableView *)tableView editActionsForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    UITableViewRowAction *renameAction = [UITableViewRowAction rowActionWithStyle:UITableViewRowActionStyleNormal title:@"rename" handler:^(UITableViewRowAction *action, NSIndexPath *indexPath){
        SCLAlertView * alert = [[SCLAlertView alloc] initWithNewWindow];
        NSLog(@"Save %@", _game.saveStates[indexPath.row]);
        UITextField *textField = [alert addTextField:@""];
        textField.text = [_game nameOfSaveStateAtIndex:indexPath.row];
        
        [alert addButton:@"Rename" actionBlock:^(void) {
            NSString *savePath = [_game pathForSaveStateAtIndex:indexPath.row];
            NSString *rootPath = savePath.stringByDeletingPathExtension.stringByDeletingPathExtension;
            NSString *dstPath = [NSString stringWithFormat:@"%@.%@.dsv", rootPath, textField.text];
            NSLog(@"%@ - \n%@", savePath, dstPath);
            
            [[NSFileManager defaultManager] moveItemAtPath:savePath toPath:dstPath error:nil];
            //[CHBgDropboxSync forceStopIfRunning]; //If we delete while syncing this shitty thing crashes
            [_game reloadSaveStates];
            [tableView reloadRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
        }];
        [alert showEdit:self title:@"Rename" subTitle:@"Rename Save State" closeButtonTitle:@"Cancel" duration:0.0f];
    }];
    renameAction.backgroundColor = [UIColor colorWithRed:85/255.0 green:175/255.0 blue:238/255.0 alpha:1];
    
    UITableViewRowAction *deleteAction = [UITableViewRowAction rowActionWithStyle:UITableViewRowActionStyleNormal title:@"Delete"  handler:^(UITableViewRowAction *action, NSIndexPath *indexPath){
        //[CHBgDropboxSync forceStopIfRunning];
        if ([_game deleteSaveStateAtIndex:indexPath.row]) {
            [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
        } else {
            NSLog(@"Error! unable to delete save state");
        }
    }];
    deleteAction.backgroundColor = [UIColor redColor];
    return @[deleteAction, renameAction];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell* cell;
    if (indexPath.section == 0) {
        if (indexPath.row == 0) {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Launch"];
            cell.textLabel.text = @"Launch Normally";
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        } else if (indexPath.row == 1) {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Launch"];
            cell.textLabel.text = @"Launch with JIT (Need iOS 8 w/ jailbreak)";
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        }
    }
    else if (indexPath.section == 1) {
        cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
        if (!cell) {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Cell"];
        }
        // Name
        NSString * saveStateTitle = [self.game nameOfSaveStateAtIndex:indexPath.row];
        if ([saveStateTitle isEqualToString:@"pause"]) {
            saveStateTitle = @"Auto Save";
        }
        cell.textLabel.text = saveStateTitle;
        
        // Date
        NSDateFormatter *timeFormatter = [[NSDateFormatter alloc]init];
        timeFormatter.dateFormat = @"h:mm a, MMMM d yyyy";
        NSString * dateString = [timeFormatter stringFromDate:[self.game dateOfSaveStateAtIndex:indexPath.row]];
        cell.detailTextLabel.text = dateString;
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    }
    return cell;
}

#pragma mark - Select ROMs

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section == 0 && indexPath.row == 1) {
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"UseJIT"];
    } else {
        [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"UseJIT"];
    }
    NSLog(@"game %@", self.game);
    [AppDelegate.sharedInstance startGame:self.game];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
}


@end
