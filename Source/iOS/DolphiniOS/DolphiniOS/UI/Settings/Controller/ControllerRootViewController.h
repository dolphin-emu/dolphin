// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ControllerRootViewController : UITableViewController

@property (strong, nonatomic) IBOutletCollection(UILabel) NSArray* m_port_name_labels;
@property (nonatomic, retain) IBOutletCollection(UILabel) NSArray* m_port_labels;
@property (strong, nonatomic) IBOutletCollection(UILabel) NSArray* m_wiimote_name_labels;
@property (strong, nonatomic) IBOutletCollection(UILabel) NSArray* m_wiimote_labels;

@end

NS_ASSUME_NONNULL_END
