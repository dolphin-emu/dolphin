//
//  AppDelegate.m
//  DolphiniOS
//
//  Created by Will Cobb on 5/20/16.
//
//
// Notes
// Changes marked by "//Will Edited"
// Changed machinecontext.h to use dummy JIT
// Commented out JITArm64_Backpatch.cpp out of build sources due to errors
// Commented out Android stuff in ControllerInterface
#import "AppDelegate.h"
#import "DolphinGame.h"
#import "EmulatorViewController.h"
#import "WCEasySettingsViewController.h"

#import "SCLAlertView.h"
#import "SSZipArchive.h"
#import "LZMAExtractor.h"
#import "ZAActivityBar.h"



//#import <UnrarKit/UnrarKit.h>



@interface AppDelegate ()

@end

@implementation AppDelegate

+ (AppDelegate*)sharedInstance
{
    return [[UIApplication sharedApplication] delegate];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:self.documentsPath]) {
        [[NSFileManager defaultManager] createDirectoryAtPath:self.documentsPath withIntermediateDirectories:YES attributes:nil error:nil];
    }
    
    
    //syscall(26, -1, 0, 0, 0);
    return YES;
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation {
    NSLog(@"Opening: %@", url);
    if ([[[NSString stringWithFormat:@"%@", url] substringToIndex:2] isEqualToString: @"db"]) {
        NSLog(@"DB");
//        if ([[DBSession sharedSession] handleOpenURL:url]) {
//            if ([[DBSession sharedSession] isLinked]) {
//                SCLAlertView * alert = [[SCLAlertView alloc] initWithNewWindow];
//                [alert showInfo:NSLocalizedString(@"SUCCESS", nil) subTitle:NSLocalizedString(@"SUCCESS_DETAIL", nil) closeButtonTitle:@"Okay!" duration:0.0];
//                [[NSUserDefaults standardUserDefaults] setBool:true forKey:@"enableDropbox"];
//                [CHBgDropboxSync clearLastSyncData];
//                [CHBgDropboxSync start];
//                [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"enableDropbox"];
//            }
//            return YES;
//        }
    } else if (url.isFileURL && [[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
        NSLog(@"Zip File (maybe)");
        NSFileManager *fm = [NSFileManager defaultManager];
        NSError *err = nil;
        if ([url.pathExtension.lowercaseString isEqualToString:@"zip"] || [url.pathExtension.lowercaseString isEqualToString:@"7z"] || [url.pathExtension.lowercaseString isEqualToString:@"rar"]) {
            
            // expand zip
            // create directory to expand
            NSString *dstDir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"extract"];
            if ([fm createDirectoryAtPath:dstDir withIntermediateDirectories:YES attributes:nil error:&err] == NO) {
                NSLog(@"Could not create directory to expand zip: %@ %@", dstDir, err);
                [fm removeItemAtURL:url error:NULL];
                [self showError:@"Unable to extract archive file."];
                [fm removeItemAtPath:url.path error:NULL];
                return NO;
            }
            
            // expand
            NSLog(@"Expanding: %@", url.path);
            NSLog(@"To: %@", dstDir);
            if ([url.pathExtension.lowercaseString isEqualToString:@"zip"]) {
                [SSZipArchive unzipFileAtPath:url.path toDestination:dstDir];
            } else if ([url.pathExtension.lowercaseString isEqualToString:@"7z"]) {
                if (![LZMAExtractor extract7zArchive:url.path tmpDirName:@"extract"]) {
                    NSLog(@"Unable to extract 7z");
                    [self showError:@"Unable to extract .7z file."];
                    [fm removeItemAtPath:url.path error:NULL];
                    return NO;
                }
            } else { //Rar
//                NSError *archiveError = nil;
//                URKArchive *archive = [[URKArchive alloc] initWithPath:url.path error:&archiveError];
//                if (!archive) {
//                    NSLog(@"Unable to open rar: %@", archiveError);
//                    [self showError:@"Unable to read .rar file."];
//                    return NO;
//                }
//                //Extract
//                NSError *error;
//                [archive extractFilesTo:dstDir overwrite:YES progress:nil error:&error];
//                if (error) {
//                    NSLog(@"Unable to extract rar: %@", archiveError);
//                    [self showError:@"Unable to extract .rar file."];
//                    [fm removeItemAtPath:url.path error:NULL];
//                    return NO;
//                }
            }
            NSLog(@"Searching");
            NSMutableArray * foundItems = [NSMutableArray array];
            NSError *error;
            // move .iNDS to documents and .dsv to battery
            for (NSString *path in [fm subpathsAtPath:dstDir]) {
                if ([path.pathExtension.lowercaseString isEqualToString:@"iso"] && ![[path.lastPathComponent substringToIndex:1] isEqualToString:@"."]) {
                    NSLog(@"found ISO in zip: %@", path);
                    [fm moveItemAtPath:[dstDir stringByAppendingPathComponent:path] toPath:[self.documentsPath stringByAppendingPathComponent:path.lastPathComponent] error:&error];
                    [foundItems addObject:path.lastPathComponent];
                } else if ([path.pathExtension.lowercaseString isEqualToString:@"dol"]) {
                    NSLog(@"found dol in zip: %@", path);
                    [fm moveItemAtPath:[dstDir stringByAppendingPathComponent:path] toPath:[self.documentsPath stringByAppendingPathComponent:path.lastPathComponent] error:&error];
                    [foundItems addObject:path.lastPathComponent];
                } else if ([path.pathExtension.lowercaseString isEqualToString:@"dsv"]) {
                    NSLog(@"found save in zip: %@", path);
                    [fm moveItemAtPath:[dstDir stringByAppendingPathComponent:path] toPath:[self.batteryDir stringByAppendingPathComponent:path.lastPathComponent] error:&error];
                    [foundItems addObject:path.lastPathComponent];
                } else if ([path.pathExtension.lowercaseString isEqualToString:@"dst"]) {
                    NSLog(@"found dst save in zip: %@", path);
                    NSString *newPath = [[path stringByDeletingPathExtension] stringByAppendingPathExtension:@"dsv"];
                    [fm moveItemAtPath:[dstDir stringByAppendingPathComponent:path] toPath:[self.batteryDir stringByAppendingPathComponent:newPath.lastPathComponent] error:&error];
                    [foundItems addObject:path.lastPathComponent];
                } else {
                    BOOL isDirectory;
                    if ([[NSFileManager defaultManager] fileExistsAtPath:[dstDir stringByAppendingPathComponent:path] isDirectory:&isDirectory]) {
                        if (!isDirectory) {
                            [[NSFileManager defaultManager] removeItemAtPath:[dstDir stringByAppendingPathComponent:path] error:NULL];
                        }
                    }
                }
                if (error) {
                    NSLog(@"Error searching archive: %@", error);
                }
            }
            if (foundItems.count == 0) {
                [self showError:@"No roms or saves found in archive. Make sure the zip contains a .nds file or a .dsv file"];
            }
            else if (foundItems.count == 1) {
                [ZAActivityBar showSuccessWithStatus:[NSString stringWithFormat:@"Added: %@", foundItems[0]] duration:3];
            } else {
                [ZAActivityBar showSuccessWithStatus:[NSString stringWithFormat:@"Added %ld items", (long)foundItems.count] duration:3];
            }
            // remove unzip dir
            [fm removeItemAtPath:dstDir error:NULL];
        } else if ([url.pathExtension.lowercaseString isEqualToString:@"iso"] || [url.pathExtension.lowercaseString isEqualToString:@"dol"]) {
            // move to documents
            [ZAActivityBar showSuccessWithStatus:[NSString stringWithFormat:@"Added: %@", url.path.lastPathComponent] duration:3];
            [fm moveItemAtPath:url.path toPath:[self.documentsPath stringByAppendingPathComponent:url.lastPathComponent] error:&err];
        } else {
            NSLog(@"Invalid File!");
            NSLog(@"%@", url.pathExtension.lowercaseString);
            [fm removeItemAtPath:url.path error:NULL];
            [self showError:@"Unable to open file: Unknown extension"];
            
        }
        [fm removeItemAtPath:url.path error:NULL];
        return YES;
    } else {
        [self showError:[NSString stringWithFormat:@"Unable to open file: Unknown Error (%i, %i, %@)", url.isFileURL, [[NSFileManager defaultManager] fileExistsAtPath:url.path], url]];
        [[NSFileManager defaultManager] removeItemAtPath:url.path error:NULL];
        
    }
    return NO;
}

- (void)showError:(NSString *)error
{
    dispatch_async(dispatch_get_main_queue(), ^{
        SCLAlertView * alertView = [[SCLAlertView alloc] init];
        alertView.shouldDismissOnTapOutside = YES;
        [alertView showError:[self topMostController] title:@"Error!" subTitle:error closeButtonTitle:@"Okay" duration:0.0];
    });
}

- (void)startGame:(DolphinGame *)game
{
    if (!self.currentEmulationController) {
        self.currentEmulationController = (EmulatorViewController *)[[UIStoryboard storyboardWithName:@"Main" bundle:nil] instantiateViewControllerWithIdentifier:@"emulatorView"];
    }
    [[self topMostController] presentViewController:self.currentEmulationController
                                                                                 animated:YES
                                                                               completion:^{
                                                                                   [self.currentEmulationController launchGame:game];
                                                                               }];
}

- (NSString *)batteryDir
{
    return [self.documentsPath stringByAppendingPathComponent:@"Battery"];
}

- (NSString *)documentsPath
{
    if ([self isSystemApplication]) {
        return [[self rootDocumentsPath] stringByAppendingPathComponent:@"Dolphin"];
    } else {
        return [self rootDocumentsPath];
    }
}

- (NSString *)rootDocumentsPath
{
    return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
}

-(BOOL)isSystemApplication {
    return [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0] pathComponents].count == 4;
}

- (UIViewController*) topMostController
{
    UIViewController *topController = [UIApplication sharedApplication].keyWindow.rootViewController;
    while (topController.presentedViewController) {
        topController = topController.presentedViewController;
    }
    return topController;
}

- (void)applicationWillResignActive:(UIApplication *)application {
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
}

- (void)applicationWillTerminate:(UIApplication *)application {
}

@end
