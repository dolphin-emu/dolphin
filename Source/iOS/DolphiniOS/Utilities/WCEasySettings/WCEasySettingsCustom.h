//
//  WCEazySettingsOption.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

@interface WCEasySettingsCustom : WCEasySettingsItem

@property NSString *subtitle;
@property UIViewController *viewController;

- (id)initWithTitle:(NSString *)title subtitle:(NSString *)subtitle viewController:(UIViewController *)controller;

@end

@interface WCEasySettingsCustomCell : WCEasySettingsItemCell {
}

@end