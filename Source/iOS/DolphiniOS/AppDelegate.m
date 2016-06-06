//
//  AppDelegate.m
//  DolphiniOS
//
//  Created by Will Cobb on 5/20/16.
//

#import "AppDelegate.h"
#import "DolphinGame.h"
#import "EmulatorViewController.h"
#import "WCEasySettingsViewController.h"

#import "SCLAlertView.h"
#import "ZAActivityBar.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

+ (AppDelegate*)sharedInstance
{
	return [[UIApplication sharedApplication] delegate];
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	// Override point for customization after application launch.
	if (![[NSFileManager defaultManager] fileExistsAtPath:self.documentsPath])
	{
		[[NSFileManager defaultManager] createDirectoryAtPath:self.documentsPath withIntermediateDirectories:YES attributes:nil error:nil];
	}

	//syscall(26, -1, 0, 0, 0); //Allows us map executable pages
	return YES;
}

- (BOOL)application:(UIApplication*)application openURL:(NSURL*)url sourceApplication:(NSString*)sourceApplication annotation:(id)annotation
{
	if (url.isFileURL && [[NSFileManager defaultManager] fileExistsAtPath:url.path])
	{
		NSLog(@"Zip File (maybe)");
		NSFileManager* fm = [NSFileManager defaultManager];
		NSError* err = nil;
		if ([url.pathExtension.lowercaseString isEqualToString:@"iso"] ||
			[url.pathExtension.lowercaseString isEqualToString:@"dol"])
		{
			// move to documents
			[ZAActivityBar showSuccessWithStatus:[NSString stringWithFormat:@"Added: %@", url.path.lastPathComponent] duration:3];
			[fm moveItemAtPath:url.path toPath:[self.documentsPath stringByAppendingPathComponent:url.lastPathComponent] error:&err];
		}
		else
		{
			NSLog(@"Invalid File!");
			NSLog(@"%@", url.pathExtension.lowercaseString);
			[fm removeItemAtPath:url.path error:NULL];
			[self showError:@"Unable to open file: Unknown extension"];
		}
		[fm removeItemAtPath:url.path error:NULL];
		return YES;
	}
	else
	{
		[self showError:[NSString stringWithFormat:@"Unable to open file: Unknown Error (%i, %i, %@)", url.isFileURL, [[NSFileManager defaultManager] fileExistsAtPath:url.path], url]];
		[[NSFileManager defaultManager] removeItemAtPath:url.path error:NULL];
	}
	return NO;
}

- (void)showError:(NSString*)error
{
	dispatch_async(dispatch_get_main_queue(), ^{
		SCLAlertView*  alertView = [[SCLAlertView alloc] init];
		alertView.shouldDismissOnTapOutside = YES;
		[alertView showError:[self topMostController] title:@"Error!" subTitle:error closeButtonTitle:@"Okay" duration:0.0];
	});
}

- (void)startGame:(DolphinGame*)game
{
	if (!self.currentEmulationController)
	{
		self.currentEmulationController = (EmulatorViewController*)[[UIStoryboard storyboardWithName:@"Main"
																							   bundle:nil]
																	 instantiateViewControllerWithIdentifier:@"emulatorView"];
	}
	[[self topMostController] presentViewController:self.currentEmulationController
											animated:YES
										 completion:^{
														[self.currentEmulationController launchGame:game];
													 }];
}

- (NSString*)batteryDir
{
	return [self.documentsPath stringByAppendingPathComponent:@"Battery"];
}

- (NSString*)documentsPath
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

- (NSString*)rootDocumentsPath
{
	return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}

-(BOOL)isSystemApplication
{
	return [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0] pathComponents].count == 4;
}

- (UIViewController*) topMostController
{
	UIViewController* topController = [UIApplication sharedApplication].keyWindow.rootViewController;
	while (topController.presentedViewController) {
		topController = topController.presentedViewController;
	}
	return topController;
}

@end
