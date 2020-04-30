// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareCollectionViewCell : UICollectionViewCell

@property (weak, nonatomic) IBOutlet UIImageView* m_image_view;
@property (weak, nonatomic) IBOutlet UILabel* m_name_label;

@end

NS_ASSUME_NONNULL_END
