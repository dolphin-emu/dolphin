//
//  WCEasySettingsSection.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WCEasySettingsSection : NSObject

@property NSArray   *items;
@property NSString  *title;
@property NSString  *subTitle;

- (id)initWithTitle:(NSString *)title subTitle:(NSString *)subTitle;

@end
