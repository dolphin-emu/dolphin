// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareVerifyResultsViewController.h"

#import "SoftwareVerifyResultsHashCell.h"
#import "SoftwareVerifyResultsProblemCell.h"

#import <vector>

@interface SoftwareVerifyResultsViewController ()

@end

@implementation SoftwareVerifyResultsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(bool)animated
{
  [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
  UIAlertController* alert_controller = [UIAlertController alertControllerWithTitle:NSLocalizedString(@"Done", nil) message:self.m_result.m_summary preferredStyle:UIAlertControllerStyleAlert];
  [alert_controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
  [self presentViewController:alert_controller animated:true completion:nil];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 3;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  switch (section)
  {
    case 0:
    {
      NSUInteger count = [self.m_result.m_problems count];
      return count == 0 ? 1 : count;
    }
    case 1:
      return 4;
    case 2:
      return 1;
    default:
      return 0;
  }
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
  switch (section)
  {
    case 0:
      return @"Problems";
    case 1:
      return @"Hashes";
    default:
      return nil;
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 0)
  {
    SoftwareVerifyResultsProblemCell* problem_cell = [tableView dequeueReusableCellWithIdentifier:@"problem_cell" forIndexPath:indexPath];
    
    if ([self.m_result.m_problems count] > 0)
    {
      SoftwareVerifyProblem* problem = [self.m_result.m_problems objectAtIndex:indexPath.row];
      [problem_cell.m_severity_label setText:problem.m_severity];
      [problem_cell.m_description_label setText:problem.m_description];
    }
    else
    {
      [problem_cell.m_severity_label setText:DOLocalizedString(@"Information")];
      [problem_cell.m_description_label setText:DOLocalizedString(@"No problems were found.")];
    }
    
    return problem_cell;
  }
  else if (indexPath.section == 1)
  {
    SoftwareVerifyResultsHashCell* hash_cell = [tableView dequeueReusableCellWithIdentifier:@"hash_cell" forIndexPath:indexPath];

    NSString* hash_str;
    switch (indexPath.row)
    {
      case 0:
        [hash_cell.m_hash_function_label setText:DOLocalizedString(@"CRC32")];
        hash_str = self.m_result.m_crc32;
        break;
      case 1:
        [hash_cell.m_hash_function_label setText:DOLocalizedString(@"MD5")];
        hash_str = self.m_result.m_md5;
        break;
      case 2:
        [hash_cell.m_hash_function_label setText:DOLocalizedString(@"SHA-1")];
        hash_str = self.m_result.m_sha1;
        break;
      case 3:
        [hash_cell.m_hash_function_label setText:DOLocalizedString(@"Redump.org Status")];
        hash_str = self.m_result.m_redump_status;
        break;
    }
    
    [hash_cell.m_hash_label setText:hash_str];
    
    return hash_cell;
  }
  else
  {
    return [tableView dequeueReusableCellWithIdentifier:@"done_cell" forIndexPath:indexPath];
  }
}

@end
