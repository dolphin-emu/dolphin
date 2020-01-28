// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SaveStateViewController.h"

#import "Core/State.h"

#import "StateCell.h"

@interface SaveStateViewController ()

@end

@implementation SaveStateViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return State::NUM_STATES;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  StateCell* cell = (StateCell*)[tableView dequeueReusableCellWithIdentifier:@"state_cell"];
  
  NSString* localizable = [NSString stringWithFormat:@"Save State Slot %ld", (long)indexPath.row + 1];
  [cell.m_slot_label setText:DOLocalizedString(localizable)];
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  State::Save((int)indexPath.row + 1);
  [self.navigationController dismissViewControllerAnimated:true completion:nil];
}

@end
