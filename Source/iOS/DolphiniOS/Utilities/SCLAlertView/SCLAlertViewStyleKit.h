//
//  SCLAlertViewStyleKit.h
//  SCLAlertView
//
//  Created by Diogo Autilio on 9/26/14.
//  Copyright (c) 2014 AnyKey Entertainment. All rights reserved.
//

#if defined(__has_feature) && __has_feature(modules)
@import Foundation;
@import UIKit;
#else
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#endif
#import "SCLButton.h"

@interface SCLAlertViewStyleKit : NSObject

// Images
/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfCheckmark;

/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfCross;

/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfNotice;

/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfWarning;

/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfInfo;

/** TODO
 *
 * TODO
 */
+ (UIImage*)imageOfEdit;

/** TODO
 *
 * TODO
 */
+ (void)drawCheckmark;

/** TODO
 *
 * TODO
 */
+ (void)drawCross;

/** TODO
 *
 * TODO
 */
+ (void)drawNotice;

/** TODO
 *
 * TODO
 */
+ (void)drawWarning;

/** TODO
 *
 * TODO
 */
+ (void)drawInfo;

/** TODO
 *
 * TODO
 */
+ (void)drawEdit;

@end
