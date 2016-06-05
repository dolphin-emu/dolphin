//
//  WCEasySettingsSwitch.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

typedef void (^ActionBlock)();

@interface WCEasySettingsSwitch : WCEasySettingsItem {
    ActionBlock onEnableBlock;
    ActionBlock onDisableBlock;
}

@property BOOL readOnlyIdentifier;

- (void)setEnableBlock:(ActionBlock)block;
- (void)setDisableBlock:(ActionBlock)block;
- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title;

- (void)onSwitch:(UISwitch *)s;

@end

@interface WCEasySettingsSwitchCell : WCEasySettingsItemCell {
    UISwitch *cellSwitch;
}

@end