//
//  WCEasySettingsViewController.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsViewController.h"

@interface WCEasySettingsViewController ()

@end

@implementation WCEasySettingsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    [self.tableView registerClass:[WCEasySettingsSwitchCell class] forCellReuseIdentifier:@"Switch"];
    [self.tableView registerClass:[WCEasySettingsSegmentCell class] forCellReuseIdentifier:@"Segment"];
    [self.tableView registerClass:[WCEasySettingsOptionCell class] forCellReuseIdentifier:@"Option"];
    [self.tableView registerClass:[WCEasySettingsSliderCell class] forCellReuseIdentifier:@"Slider"];
    [self.tableView registerClass:[WCEasySettingsUrlCell class] forCellReuseIdentifier:@"Url"];
    [self.tableView registerClass:[WCEasySettingsCustomCell class] forCellReuseIdentifier:@"Custom"];
    if([self.tableView respondsToSelector:@selector(setCellLayoutMarginsFollowReadableWidth:)]) {
        self.tableView.cellLayoutMarginsFollowReadableWidth = NO;
    }
}

- (void)viewWillAppear:(BOOL)animated
{
    [self.tableView reloadData];
    NSLog(@"%@", self.navigationController.viewControllers);
    if (self.navigationController.viewControllers.count == 1) { // We are the main view so add an exit
        UIBarButtonItem *doneButton = [[UIBarButtonItem alloc]
                                       initWithTitle:@"Done"
                                       style:UIBarButtonItemStyleDone
                                       target:self
                                       action:@selector(done)];
        self.navigationItem.leftBarButtonItem = doneButton;
    } else {
        self.navigationItem.leftBarButtonItem = nil; 
    }
}

- (void)done
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
}

- (WCEasySettingsItem *)itemForIndexPath:(NSIndexPath *)indexPath
{
    WCEasySettingsSection *section = self.sections[indexPath.section];
    return section.items[indexPath.row];
}

- (void)pushViewController:(UIViewController *)controller
{
    [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - Table view data source

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    WCEasySettingsSection *s = self.sections[section];
    return s.title;
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section
{
    
    WCEasySettingsSection *s = self.sections[section];
    return s.subTitle;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    WCEasySettingsItem *item = [self itemForIndexPath:indexPath];
    item.delegate = self;
    WCEasySettingsItemCell *cell = [tableView dequeueReusableCellWithIdentifier:item.cellIdentifier];
    
    cell.controller = item;
    return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    WCEasySettingsItem *item = [self itemForIndexPath:indexPath];
    if (item.type == WCEasySettingsTypeSegment || item.type == WCEasySettingsTypeSlider) {
        return 74;
    }
    return 44;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return self.sections.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    WCEasySettingsSection *settignsSection = self.sections[section];
    return settignsSection.items.count;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    WCEasySettingsItem *item = [self itemForIndexPath:indexPath];
    [item itemSelected];
}





@end
