// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "Bridge/DolphinGame.h"

#import "UI/GameTableView.h"
#import "UI/EmulatorViewController.h"

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
- (BOOL)tableView:(UITableView*)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath
{
	return indexPath.section != 0;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
	if (section == 0)
		return 2;
	return 0;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
	return 2;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
	UITableViewCell* cell;
	if (indexPath.section == 0)
	{
		if (indexPath.row == 0)
		{
			cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Launch"];
			cell.textLabel.text = @"Launch Normally";
			cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
		}
		else if (indexPath.row == 1)
		{
			cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Launch"];
			cell.textLabel.text = @"Launch with JIT (Needs jailbreak)";
			cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
		}
	}
	else if (indexPath.section == 1)
	{
		// ToDo: Show savestates here
	}
	return cell;
}

#pragma mark - Select ROMs

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
	[tableView deselectRowAtIndexPath:indexPath animated:YES];
	if (indexPath.section == 0 && indexPath.row == 0)
	{
		[[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"UseJIT"];
	}
	else
	{
		[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"UseJIT"];
	}
	EmulatorViewController* emulatorView = [[EmulatorViewController alloc] init];
	[self presentViewController:emulatorView animated:YES completion:^{
		[emulatorView launchGame:self.game];
	}];
	
}

@end
