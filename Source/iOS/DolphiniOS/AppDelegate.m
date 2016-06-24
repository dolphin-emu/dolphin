// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "AppDelegate.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	if (![[NSFileManager defaultManager] fileExistsAtPath:[AppDelegate documentsPath]])
	{
		[[NSFileManager defaultManager] createDirectoryAtPath:[AppDelegate documentsPath]
		                          withIntermediateDirectories:YES
		                                           attributes:nil
		                                                error:nil];
	}

	//syscall(26, -1, 0, 0, 0); //Allows mapping executable pages for JIT
	return YES;
}

- (BOOL)application:(UIApplication*)application openURL:(NSURL*)url sourceApplication:(NSString*)sourceApplication annotation:(id)annotation
{
	if (url.isFileURL && [[NSFileManager defaultManager] fileExistsAtPath:url.path])
	{
		NSFileManager* fm = [NSFileManager defaultManager];
		if ([url.pathExtension.lowercaseString isEqualToString:@"iso"] ||
		    [url.pathExtension.lowercaseString isEqualToString:@"dol"])
		{
			[fm moveItemAtPath:url.path toPath:[[AppDelegate documentsPath] stringByAppendingPathComponent:url.lastPathComponent] error:nil];
			return YES;
		}
		else
		{
			NSLog(@"Invalid File: %@", url.pathExtension.lowercaseString);
		}
	}
	else
	{
		NSLog(@"Unable to open file: %@", url.path);
	}
	[[NSFileManager defaultManager] removeItemAtPath:url.path error:NULL];
	return NO;
}

+(NSString*)libraryPath
{
	return [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}

+(NSString*)documentsPath
{
	if ([self isSystemApplication])
	{
		return [[self rootDocumentsPath] stringByAppendingPathComponent:@"Dolphin"];
	}
	else
	{
		return [self rootDocumentsPath];
	}
}

+(NSString*)rootDocumentsPath
{
	return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}

+(BOOL)isSystemApplication
{
	return [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0] pathComponents].count == 4;
}


@end
