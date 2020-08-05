// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareActionReplayEditViewController.h"

#import "Core/ARDecrypt.h"

@interface SoftwareActionReplayEditViewController ()

@end

@implementation SoftwareActionReplayEditViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  if (self.m_ar_code == nullptr)
  {
    self.m_ar_code = new ActionReplay::ARCode;
    self.m_ar_code->user_defined = true;
    self.m_should_be_pushed_back = true;
  }
  
  [self.m_name_field setText:CppToFoundationString(self.m_ar_code->name)];
  
  NSMutableString* code_str = [[NSMutableString alloc] init];
  for (ActionReplay::AREntry& e : self.m_ar_code->ops)
  {
    [code_str appendFormat:@"%08X %08X\n", e.cmd_addr, e.value];
  }
  
  NSString* trimmed_code_str = [code_str length] > 0 ? [code_str substringToIndex:[code_str length] - 1] : code_str;
  [self.m_code_view setText:trimmed_code_str];
}

#pragma mark - Table view delegate

- (void)textViewDidChange:(UITextView*)textView
{
  dispatch_async(dispatch_get_main_queue(), ^{
    [self.tableView beginUpdates];
    [self.tableView endUpdates];
  });
}

#pragma mark - Done button

- (IBAction)DonePressed:(id)sender
{
  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;
  
  NSArray<NSString*>* code_array = [self.m_code_view.text componentsSeparatedByString:@"\n"];
  
  for (NSUInteger i = 0; i < [code_array count]; i++)
  {
    NSString* line = code_array[i];
    
    NSString* trimmed_line = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([trimmed_line length] == 0)
    {
      continue;
    }
    
    NSArray<NSString*>* values = [line componentsSeparatedByString:@" "];
    
    bool good = true;

    u32 addr = 0;
    u32 value = 0;

    if ([values count] == 2)
    {
      // Create NSCharacterSet for hexadecimal values
      NSCharacterSet* hex_set = [[NSCharacterSet characterSetWithCharactersInString:@"ABCDEFabcdef0123456789"] invertedSet];
      
      if ([values[0] rangeOfCharacterFromSet:hex_set].location == NSNotFound && [values[1] rangeOfCharacterFromSet:hex_set].location == NSNotFound)
      {
        // Too lazy to allocate NSScanner, so we use std::stoul here
        addr = (u32)std::stoul([values[0] UTF8String], nullptr, 16);
        value = (u32)std::stoul([values[1] UTF8String], nullptr, 16);
        
        entries.push_back(ActionReplay::AREntry(addr, value));
      }
      else
      {
        good = false;
      }
    }
    else
    {
      NSArray<NSString*>* blocks = [line componentsSeparatedByString:@"-"];

      if ([blocks count] == 3 && [blocks[0] length] == 4 && [blocks[1] length] == 4 &&
          [blocks[2] length] == 5)
      {
        encrypted_lines.emplace_back(FoundationToCppString(blocks[0]) + FoundationToCppString(blocks[1]) +
                                     FoundationToCppString(blocks[2]));
      }
      else
      {
        good = false;
      }
    }

    if (!good)
    {
      // TODO: PC Dolphin lets you continue parsing here
      NSString* parsing_err_string = DOLocalizedStringWithArgs(@"Unable to parse line %1 of the entered AR code as a valid "
                                                       "encrypted or decrypted code. Make sure you typed it correctly.", @"d");
      parsing_err_string = [NSString stringWithFormat:parsing_err_string, i + 1];
      
      UIAlertController* parsing_err_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Parsing Error") message:parsing_err_string preferredStyle:UIAlertControllerStyleAlert];
      [parsing_err_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
      
      [self presentViewController:parsing_err_alert animated:true completion:nil];
      
      return;
    }
  }

  if (!encrypted_lines.empty())
  {
    if (!entries.empty())
    {
      // TODO: PC Dolphin lets you choose between the following:
      // YES = Discard the unencrypted lines then decrypt the encrypted ones instead.
      // NO = Discard the encrypted lines, keep the unencrypted ones
      // CANCEL = Stop and let the user go back to editing
      
      NSString* mixed_err_string = @"This Action Replay code contains both encrypted and unencrypted lines; "
                                    "you should check that you have entered it correctly.";
      
      UIAlertController* mixed_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Invalid Mixed Code") message:mixed_err_string preferredStyle:UIAlertControllerStyleAlert];
      [mixed_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
      
      [self presentViewController:mixed_alert animated:true completion:nil];
      
      return;
    }
    
    ActionReplay::DecryptARCode(encrypted_lines, &entries);
  }

  if (entries.empty())
  {
    UIAlertController* empty_error = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Error") message:DOLocalizedString(@"The resulting decrypted AR code doesn't contain any lines.") preferredStyle:UIAlertControllerStyleAlert];
    [empty_error addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:empty_error animated:true completion:nil];
    
    return;
  }
  
  if (!self.m_ar_code->user_defined)
  {
    self.m_ar_code = new ActionReplay::ARCode;
    self.m_should_be_pushed_back = true;
  }

  self.m_ar_code->name = FoundationToCppString(self.m_name_field.text);
  self.m_ar_code->ops = std::move(entries);
  self.m_ar_code->user_defined = true;
  
  [self performSegueWithIdentifier:@"unwind_to_ar" sender:nil];
}

@end
