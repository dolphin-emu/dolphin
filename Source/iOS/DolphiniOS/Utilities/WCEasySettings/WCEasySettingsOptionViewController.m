
//
//  WCEasySettingsOptionViewController.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsOptionViewController.h"

@interface WCEasySettingsOptionViewController ()

@end

@implementation WCEasySettingsOptionViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    if([self.tableView respondsToSelector:@selector(setCellLayoutMarginsFollowReadableWidth:)]) {
        self.tableView.cellLayoutMarginsFollowReadableWidth = NO;
    }
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
}

#pragma mark - Table view data source

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section
{
    return self.option.subtitle;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Option"];
    if (!cell) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"Option"];
    }
    cell.textLabel.text = self.option.options[indexPath.row];
    if (self.option.optionSubtitles.count > indexPath.row) {
        cell.detailTextLabel.text = self.option.optionSubtitles[indexPath.row];
    } else {
        cell.detailTextLabel.text = @"";
    }
    
    
    
    NSInteger selectedIndex = [[NSUserDefaults standardUserDefaults] integerForKey:self.option.identifier];
    if (indexPath.row == selectedIndex) {
        cell.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
        cell.accessoryType = UITableViewCellAccessoryNone;
    }
    
    return cell;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return self.option.options.count;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    [[NSUserDefaults standardUserDefaults] setInteger:indexPath.row forKey:self.option.identifier];
    [self.navigationController popViewControllerAnimated:YES];
}


@end
