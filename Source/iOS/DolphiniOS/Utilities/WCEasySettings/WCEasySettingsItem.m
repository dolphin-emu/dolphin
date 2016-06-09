//
//  WCEasySettingsItem.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

@implementation WCEasySettingsItem

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title
{
    if (self = [super init]) {
        self.identifier = identifier;
        self.title = title;
    }
    return self;
}

- (void)itemSelected
{
    
}

@end

@implementation WCEasySettingsItemCell

- (void)setFrame:(CGRect)frame
{
    [super setFrame:frame];
}

@end
