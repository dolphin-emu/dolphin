//
//  WCEazySettingsOption.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

@interface WCEasySettingsUrl : WCEasySettingsItem

@property NSString *url;
@property NSString *subtitle;

- (id)initWithTitle:(NSString *)title subtitle:(NSString *)subtitle url:(NSString *)url;

@end

@interface WCEasySettingsUrlCell : WCEasySettingsItemCell {
}

@end