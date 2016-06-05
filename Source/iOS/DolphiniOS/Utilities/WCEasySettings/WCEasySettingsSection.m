//
//  WCEasySettingsSection.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsSection.h"

@implementation WCEasySettingsSection

- (id)initWithTitle:(NSString *)title subTitle:(NSString *)subTitle
{
    if (self = [super init]) {
        self.title = title;
        self.subTitle = subTitle;
    }
    return self;
}

@end
