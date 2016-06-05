//
//  WCEazySettingsOption.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"


@interface WCEasySettingsOption : WCEasySettingsItem {
}

@property NSArray *options;
@property NSArray *optionSubtitles;
@property NSString *subtitle;

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title options:(NSArray *)options optionSubtitles:(NSArray *)optionSubtitles subtitle:(NSString *)subtitle;

@end

@interface WCEasySettingsOptionCell : WCEasySettingsItemCell {
}

@end