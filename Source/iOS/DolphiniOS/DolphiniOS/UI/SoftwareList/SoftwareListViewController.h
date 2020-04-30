// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "UICommon/GameFileCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareListViewController : UIViewController <UITableViewDelegate, UITableViewDataSource, UICollectionViewDelegate, UICollectionViewDataSource>

@property(nonatomic) UICommon::GameFileCache* m_cache;
@property(nonatomic) bool m_cache_loaded;
@property (weak, nonatomic) IBOutlet UITableView* m_table_view;
@property (weak, nonatomic) IBOutlet UICollectionView* m_collection_view;

@end

NS_ASSUME_NONNULL_END
