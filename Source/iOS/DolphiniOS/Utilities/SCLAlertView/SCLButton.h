//
//  SCLButton.h
//  SCLAlertView
//
//  Created by Diogo Autilio on 9/26/14.
//  Copyright (c) 2014 AnyKey Entertainment. All rights reserved.
//

#if defined(__has_feature) && __has_feature(modules)
@import UIKit;
#else
#import <UIKit/UIKit.h>
#endif

@class SCLTimerDisplay;

@interface SCLButton : UIButton

typedef void (^SCLActionBlock)(void);
typedef BOOL (^SCLValidationBlock)(void);
typedef NSDictionary* (^CompleteButtonFormatBlock)(void);
typedef NSDictionary* (^ButtonFormatBlock)(void);

// Action Types
typedef NS_ENUM(NSInteger, SCLActionType)
{
    SCLNone,
    SCLSelector,
    SCLBlock
};

/** Set button action type.
 *
 * Holds the button action type.
 */
@property SCLActionType actionType;

/** Set action button block.
 *
 * TODO
 */
@property (nonatomic, copy) SCLActionBlock actionBlock;

/** Set Validation button block.
 *
 * Set one kind of validation and keeps the alert visible until the validation is successful
 */
@property (nonatomic, copy) SCLValidationBlock validationBlock;

/** Set Complete button format block.
 *
 * Holds the complete button format block.
 * Support keys : backgroundColor, borderWidth, borderColor, textColor
 */
@property (nonatomic, copy) CompleteButtonFormatBlock completeButtonFormatBlock;

/** Set button format block.
 *
 * Holds the button format block.
 * Support keys : backgroundColor, borderWidth, borderColor, textColor
 */
@property (nonatomic, copy) ButtonFormatBlock buttonFormatBlock;

/** Set SCLButton color.
 *
 * Set SCLButton color.
 */
@property (nonatomic, strong) UIColor *defaultBackgroundColor;

/** Set Target object.
 *
 * Target is an object that holds the information necessary to send a message to another object when an event occurs.
 */
@property id target;

/** Set selector id.
 *
 * A selector is the name used to select a method to execute for an object,
 * or the unique identifier that replaces the name when the source code is compiled.
 */
@property SEL selector;

/** Parse button configuration
 *
 * Parse ButtonFormatBlock and CompleteButtonFormatBlock setting custom configuration.
 * Set keys : backgroundColor, borderWidth, borderColor, textColor
 */
- (void)parseConfig:(NSDictionary *)buttonConfig;

/** Set button timer.
 *
 * Holds the button timer, if present.
 */
@property (nonatomic) SCLTimerDisplay *timer;

/** Init method
 *
 */
- (instancetype)initWithWindowWidth:(CGFloat)windowWidth;

@end
