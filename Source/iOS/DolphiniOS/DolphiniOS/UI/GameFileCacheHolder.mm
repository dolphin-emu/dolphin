// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GameFileCacheHolder.h"

#import "MainiOS.h"

@implementation GameFileCacheHolder

+ (GameFileCacheHolder*)sharedInstance
{
  static dispatch_once_t _once_token = 0;
  static GameFileCacheHolder* _sharedHolder = nil;
  
  dispatch_once(&_once_token, ^{
      _sharedHolder = [[self alloc] init];
  });
  
  return _sharedHolder;
}

-(id)init
{
  if (self = [super init])
  {
    self.m_cache = new UICommon::GameFileCache();
    self.m_cache->Load();
  }
  return self;
}

- (void)rescanWithCompletionHandler:(nullable void (^)())completion_handler
{
  // Get the software folder path
  NSString* softwareDirectory = [[MainiOS getUserFolder] stringByAppendingPathComponent:@"Software"];

  // Create it if necessary
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:softwareDirectory])
  {
    [fileManager createDirectoryAtPath:softwareDirectory withIntermediateDirectories:false attributes:nil error:nil];
  }
  
  std::vector<std::string> folder_paths;
  folder_paths.push_back(std::string([softwareDirectory UTF8String]));
  
  // Update the cache
  bool cache_updated = self.m_cache->Update(UICommon::FindAllGamePaths(folder_paths, false));
  if (cache_updated)
  {
    self.m_cache->Save();
  }
  
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^
  {
    self.m_cache->UpdateAdditionalMetadata();
    
    if (completion_handler)
    {
      completion_handler();
    }
  });
}

@end
