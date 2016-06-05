//
//  WCEasySettingsViewController.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "WCEasySettingsItem.h"
#import "WCEasySettingsSwitch.h"
#import "WCEasySettingsSegment.h"
#import "WCEasySettingsSlider.h"
#import "WCEasySettingsSection.h"
#import "WCEasySettingsOption.h"
#import "WCEasySettingsUrl.h"
#import "WCEasySettingsCustom.h"

@interface WCEasySettingsViewController : UITableViewController <WCEasySettingsItemDelegate>

@property NSArray *sections;

@end
