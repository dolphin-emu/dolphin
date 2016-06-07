// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "AppDelegate.h"
#import "MHWDirectoryWatcher.h"

#import "Bridge/DolphinGame.h"
#import "UI/RomTableViewController.h"
#import "UI/GameTableView.h"

@interface RomTableViewController ()
{
	MHWDirectoryWatcher* docWatchHelper;
	NSArray* games;
}

@end

@implementation RomTableViewController

- (void)viewDidLoad
{
	[super viewDidLoad];
	self.navigationItem.title = @"ROMs";

	[self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:@"DolphinGame"];

	docWatchHelper = [MHWDirectoryWatcher directoryWatcherAtPath:[AppDelegate documentsPath]
														callback:^{
															[self reloadGames:self];
														}];
}

- (void)didReceiveMemoryWarning
{
	[super didReceiveMemoryWarning];
}

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];
	[self reloadGames:nil];
}

- (void)viewDidAppear:(BOOL)animated {
	[super viewWillAppear:animated];
}

- (void)reloadGames:(id)sender
{
	NSLog(@"Reloading");
	if (sender == self)
	{
		// do it later, the file may not be written yet
		[self performSelector:_cmd withObject:nil afterDelay:1];
	}
	else
	{
		games = [DolphinGame gamesAtPath:[AppDelegate documentsPath]];
		NSString* romsPath = [[NSBundle mainBundle] resourcePath];
		games = [games arrayByAddingObjectsFromArray:[DolphinGame gamesAtPath:romsPath]];
		dispatch_async(dispatch_get_main_queue(), ^{
			[self.tableView reloadData];
		});
	}
}

#pragma mark - Table View

- (BOOL)tableView:(UITableView*)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath
{
	return YES;
}

- (void)tableView:(UITableView*)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath*)indexPath
{
	if (editingStyle == UITableViewCellEditingStyleDelete)
	{
		// Delete game
		DolphinGame* game = games[indexPath.row];
		if ([[NSFileManager defaultManager] removeItemAtPath:game.path error:NULL])
		{
			[self reloadGames:nil];
			[self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
		}
	}
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
	return games.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
	return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
	UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"DolphinGame"];
	DolphinGame* game = games[indexPath.row];

	if (game.gameTitle)
	{
		// use title from ROM
		cell.textLabel.text = game.gameTitle;
		cell.detailTextLabel.text = game.gameSubtitle;
	}
	else
	{
		// use filename
		cell.textLabel.text = game.title;
		cell.detailTextLabel.text = nil;
	}
	cell.imageView.image = game.banner;
	cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
	return cell;
}

#pragma mark - Select ROMs

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
	[tableView deselectRowAtIndexPath:indexPath animated:YES];
	if (indexPath.section == 0)
	{
		DolphinGame* game = games[indexPath.row];
		GameTableView* gameInfo = [[GameTableView alloc] init];
		gameInfo.game = game;
		[self.navigationController pushViewController:gameInfo animated:YES];
	}
}

@end
