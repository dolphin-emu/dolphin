//
//  SCLAlertViewResponder.h
//  SCLAlertView
//
//  Created by Diogo Autilio on 9/26/14.
//  Copyright (c) 2014 AnyKey Entertainment. All rights reserved.
//

#if defined(__has_feature) && __has_feature(modules)
@import Foundation;
#else
#import <Foundation/Foundation.h>
#endif
#import "SCLAlertView.h"

@interface SCLAlertViewResponder : NSObject

/** TODO
 *
 * TODO
 */
- (instancetype)init:(SCLAlertView *)alertview;

/** TODO
 *
 * TODO
 */
- (void)close;

@end
