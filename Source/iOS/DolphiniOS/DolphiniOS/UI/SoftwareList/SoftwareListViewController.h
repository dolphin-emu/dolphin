// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "DiscIO/Enums.h"

#import "UICommon/GameFileCache.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, DOLBootType)
{
  DOLBootTypeGame,
  DOLBootTypeWiiMenu,
  DOLBootTypeGCIPL
};

@interface SoftwareListViewController : UIViewController <UITableViewDelegate, UITableViewDataSource, UICollectionViewDelegate, UICollectionViewDataSource, UIDocumentPickerDelegate>

@property(nonatomic) UICommon::GameFileCache* m_cache;
@property(nonatomic) bool m_cache_loaded;
@property(nonatomic) const UICommon::GameFile* m_selected_file;
@property(nonatomic) DiscIO::Region m_ipl_region;
@property(nonatomic) DOLBootType m_boot_type;
@property (weak, nonatomic) IBOutlet UINavigationItem* m_navigation_item;
@property (weak, nonatomic) IBOutlet UIBarButtonItem* m_menu_button;
@property (nonatomic) IBOutlet UIBarButtonItem* m_grid_button;
@property (nonatomic) IBOutlet UIBarButtonItem* m_list_button;
@property (weak, nonatomic) IBOutlet UIBarButtonItem* m_add_button;
@property (weak, nonatomic) IBOutlet UITableView* m_table_view;
@property (weak, nonatomic) IBOutlet UICollectionView* m_collection_view;

@end

NS_ASSUME_NONNULL_END
