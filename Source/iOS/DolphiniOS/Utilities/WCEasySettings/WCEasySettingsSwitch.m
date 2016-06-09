//
//  WCEasySettingsSwitch.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsSwitch.h"

@implementation WCEasySettingsSwitch

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title
{
    if (self = [super initWithIdentifier:identifier title:title]) {
        self.cellIdentifier = @"Switch";
        self.type = WCEasySettingsTypeSwitch;
    }
    return self;
}

- (void)onSwitch:(UISwitch *)s
{
    if (!self.readOnlyIdentifier)
        [[NSUserDefaults standardUserDefaults] setBool:s.on forKey:self.identifier];
    if (s.on && onEnableBlock) {
        onEnableBlock();
    } else if (!s.on && onDisableBlock) {
        onDisableBlock();
    }
}

- (void)setEnableBlock:(ActionBlock)block
{
    onEnableBlock = block;
}

- (void)setDisableBlock:(ActionBlock)block
{
    onDisableBlock = block;
}

@end


@implementation WCEasySettingsSwitchCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
        cellSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(self.frame.size.width - 70, 6, 51, 31)];
        cellSwitch.tintColor = [[[UIApplication sharedApplication] delegate] window].tintColor;
        cellSwitch.onTintColor = [[[UIApplication sharedApplication] delegate] window].tintColor;
        [self.contentView addSubview:cellSwitch];
    }
    return self;
}

- (void)setFrame:(CGRect)frame
{
    [super setFrame:frame];
    cellSwitch.frame = CGRectMake(frame.size.width - 70, 6, 51, 31);
}

- (void)setController:(WCEasySettingsSwitch *)controller
{
    self.textLabel.text = controller.title;
    
    [cellSwitch removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
    [cellSwitch addTarget:controller action:@selector(onSwitch:) forControlEvents:UIControlEventValueChanged];
    cellSwitch.on = [[NSUserDefaults standardUserDefaults] boolForKey:controller.identifier];
    
    self.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

@end
