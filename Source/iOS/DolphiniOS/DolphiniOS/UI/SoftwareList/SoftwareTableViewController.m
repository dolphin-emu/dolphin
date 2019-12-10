// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DolphiniOS-Swift.h"
#import "SoftwareTableViewController.h"
#import "SoftwareTableViewCell.h"

#import <MetalKit/MetalKit.h>

@interface SoftwareTableViewController ()

@end

@implementation SoftwareTableViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Get the software folder path
  NSString* userDirectory =
      [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)
          objectAtIndex:0];
  self.softwareDirectory = [userDirectory stringByAppendingPathComponent:@"Software"];

  // Create if necessary
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:self.softwareDirectory])
  {
    [fileManager createDirectoryAtPath:self.softwareDirectory
           withIntermediateDirectories:false
                            attributes:NULL
                                 error:NULL];
  }

  // Get all files within folder
  self.softwareFiles =
      [[NSFileManager defaultManager] subpathsOfDirectoryAtPath:self.softwareDirectory error:NULL];
}

- (void)viewDidAppear:(BOOL)animated
{
#ifndef SUPPRESS_UNSUPPORTED_DEVICE
  // Check for GPU Family 3
  id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
  if (![metalDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Unsupported Device"
                                   message:@"DolphiniOS can only run on devices with an A9 processor or newer.\n\nThis is because your device's GPU does not support a feature required by Dolphin for good performance. Your device would run Dolphin at an unplayable speed without the feature."
                                   preferredStyle:UIAlertControllerStyleAlert];
     
    UIAlertAction* okayAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
       handler:^(UIAlertAction * action) {
      exit(0);
    }];
     
    [alert addAction:okayAction];
    [self presentViewController:alert animated:YES completion:nil];
  }
#endif
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self.softwareFiles.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareTableViewCell* cell =
      (SoftwareTableViewCell*)[tableView dequeueReusableCellWithIdentifier:@"softwareCell"
                                                              forIndexPath:indexPath];

  // Set the file name
  cell.fileNameLabel.text = [[self.softwareFiles objectAtIndex:indexPath.item] lastPathComponent];

  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSString* filePath = [self.softwareDirectory
      stringByAppendingPathComponent:[self.softwareFiles objectAtIndex:indexPath.item]];
  EmulationViewController* viewController = [[EmulationViewController alloc] initWithFile:filePath];
  [self presentViewController:viewController animated:YES completion:nil];
}

/*
// Override to support conditional editing of the table view.
- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the specified item to be editable.
    return YES;
}
*/

/*
// Override to support editing the table view.
- (void)tableView:(UITableView *)tableView
commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath
*)indexPath { if (editingStyle == UITableViewCellEditingStyleDelete) {
        // Delete the row from the data source
        [tableView deleteRowsAtIndexPaths:@[indexPath]
withRowAnimation:UITableViewRowAnimationFade]; } else if (editingStyle ==
UITableViewCellEditingStyleInsert) {
        // Create a new instance of the appropriate class, insert it into the array, and add a new
row to the table view
    }
}
*/

/*
// Override to support rearranging the table view.
- (void)                        :(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath
*)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath {
}
*/

/*
// Override to support conditional rearranging of the table view.
- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the item to be re-orderable.
    return YES;
}
*/

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before
navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
