// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "Bridge/DolphinGame.h"

NSString* const DolphinGameSaveStatesChangedNotification = @"DolphinGameSaveStatesChangedNotification";

@implementation DolphinGame

+ (NSArray*)gamesAtPath:(NSString*)gamesPath
{
	NSMutableArray* games = [NSMutableArray new];
	NSArray* files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:gamesPath error:NULL];
	for(NSString* file in files)
	{
		if ([file.pathExtension isEqualToString:@"iso"] ||
		    [file.pathExtension isEqualToString:@"dol"])
		{
			DolphinGame* game = [[DolphinGame alloc]initWithPath:[gamesPath stringByAppendingPathComponent:file]];
			if (game)
			{
				[games addObject:game];
			}
		}
		else
		{
			// Recurse
			BOOL isDirectory;
			if ([[NSFileManager defaultManager] fileExistsAtPath:[gamesPath stringByAppendingPathComponent:file]
			                                         isDirectory:&isDirectory] &&
				isDirectory)
			{
				[games addObjectsFromArray:[DolphinGame gamesAtPath:[gamesPath stringByAppendingPathComponent:file]]];
			}
		}
	}
	return [NSArray arrayWithArray:games];
}

- (DolphinGame*)initWithPath:(NSString*)path
{
	NSAssert(path.isAbsolutePath, @"DolphinGame path must be absolute");
	if (!([path.pathExtension.lowercaseString isEqualToString:@"iso"] ||
	      [path.pathExtension.lowercaseString isEqualToString:@"dol"]))
	{
		return nil;
	}

	// Make sure file exists
	BOOL isDirectory = NO;
	if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] ||
		isDirectory)
	{
		return nil;
	}
	if ((self = [super init]))
	{
		self.path = path;
	}
	return self;
}

- (NSString*)title
{
	return self.path.lastPathComponent.stringByDeletingPathExtension;
}

+ (int)preferredLanguage
{
	NSString* preferredLanguage = [[NSLocale preferredLanguages][0] substringToIndex:2];
	NSUInteger pos = [@[@"ja",@"en",@"fr",@"de",@"it",@"es",@"zh"] indexOfObject:preferredLanguage];
	if (pos == NSNotFound)
		return 1; // Default to english
	return (int)pos;
}

- (NSString*)gameTitle
{
	return nil;
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<DolphinGame 0x%p: %@>", self, self.path];
}


@end
