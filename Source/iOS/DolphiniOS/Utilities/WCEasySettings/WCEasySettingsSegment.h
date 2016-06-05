//
//  WCEasySettingsSegment.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

@interface WCEasySettingsSegment : WCEasySettingsItem

@property NSArray *items;

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title items:(NSArray *)items;

@end

@interface WCEasySettingsSegmentCell : WCEasySettingsItemCell {
    UILabel             *cellTitle;
    UISegmentedControl  *cellSegment;
}


@end