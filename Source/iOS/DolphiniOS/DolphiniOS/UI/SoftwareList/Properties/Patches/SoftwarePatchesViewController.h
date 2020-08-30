// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import <vector>

namespace PatchEngine
{
struct Patch;
}

namespace UICommon
{
class GameFile;
}

NS_ASSUME_NONNULL_BEGIN

@interface SoftwarePatchesViewController : UITableViewController
{
  std::vector<PatchEngine::Patch> m_patches;
}

@property(nonatomic) UICommon::GameFile* m_game_file;

@end

NS_ASSUME_NONNULL_END
