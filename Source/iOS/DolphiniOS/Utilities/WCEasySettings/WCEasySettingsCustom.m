//
//  WCEazySettingsOption.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsCustom.h"
@implementation WCEasySettingsCustom

- (id)initWithTitle:(NSString *)title subtitle:(NSString *)subtitle viewController:(UIViewController *)controller
{
    if (self = [super initWithIdentifier:nil title:title]) {
        self.subtitle = subtitle;
        self.type = WCEasySettingsTypeCustom;
        self.cellIdentifier = @"Custom";
        self.viewController = controller;
    }
    return self;
}


- (void)itemSelected
{
    [self.delegate pushViewController:self.viewController];
}

@end

@implementation WCEasySettingsCustomCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:reuseIdentifier]) {
        
    }
    return self;
}

- (void)setController:(WCEasySettingsCustom *)controller
{
    self.textLabel.text = controller.title;
    
    self.detailTextLabel.text = controller.subtitle;
    
    self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

@end
