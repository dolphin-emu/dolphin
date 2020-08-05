// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareGeckoEditViewController.h"

@interface SoftwareGeckoEditViewController ()

@end

@implementation SoftwareGeckoEditViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  if (self.m_gecko_code == nullptr)
  {
    self.m_gecko_code = new Gecko::GeckoCode;
    self.m_gecko_code->user_defined = true;
    self.m_should_be_pushed_back = true;
  }
  
  [self.m_name_field setText:CppToFoundationString(self.m_gecko_code->name)];
  [self.m_creator_field setText:CppToFoundationString(self.m_gecko_code->creator)];
  
  NSMutableString* code_str = [[NSMutableString alloc] init];
  for (Gecko::GeckoCode::Code& c : self.m_gecko_code->codes)
  {
    [code_str appendFormat:@"%08X %08X\n", c.address, c.data];
  }
  
  NSString* trimmed_code_str = [code_str length] > 0 ? [code_str substringToIndex:[code_str length] - 1] : code_str;
  [self.m_code_view setText:trimmed_code_str];
  
  NSMutableString* notes_str = [[NSMutableString alloc] init];
  for (std::string& line : self.m_gecko_code->notes)
  {
    [notes_str appendFormat:@"%@\n", CppToFoundationString(line)];
  }
  
  NSString* trimmed_notes_str = [notes_str length] > 0 ? [notes_str substringToIndex:[notes_str length] - 1] : notes_str;
  [self.m_notes_view setText:trimmed_notes_str];
}

#pragma mark - Text view delegate

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
  std::vector<Gecko::GeckoCode::Code> entries;
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
    
    // Create NSCharacterSet for hexadecimal values
    NSCharacterSet* hex_set = [[NSCharacterSet characterSetWithCharactersInString:@"ABCDEFabcdef0123456789"] invertedSet];
    
    bool good = [values count] == 2 && [values[0] rangeOfCharacterFromSet:hex_set].location == NSNotFound && [values[1] rangeOfCharacterFromSet:hex_set].location == NSNotFound;
    
    if (!good)
    {
      // TODO: PC Dolphin lets you continue parsing here
      NSString* parsing_err_string = DOLocalizedStringWithArgs(@"Unable to parse line %1 of the entered Gecko code as a valid "
                                                                "code. Make sure you typed it correctly.", @"d");
      parsing_err_string = [NSString stringWithFormat:parsing_err_string, i + 1];
      
      UIAlertController* parsing_err_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Parsing Error") message:parsing_err_string preferredStyle:UIAlertControllerStyleAlert];
      [parsing_err_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
      
      [self presentViewController:parsing_err_alert animated:true completion:nil];
      
      return;
    }
    else
    {
      // Too lazy to allocate NSScanner, so we use std::stoul here
      u32 addr = (u32)std::stoul([values[0] UTF8String], nullptr, 16);
      u32 value = (u32)std::stoul([values[1] UTF8String], nullptr, 16);
      
      Gecko::GeckoCode::Code c;
      c.address = addr;
      c.data = value;
      c.original_line = FoundationToCppString(line);
      
      entries.push_back(c);
    }
  }

  if (entries.empty())
  {
    // TODO: "AR Code"?
    UIAlertController* empty_error = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Error") message:DOLocalizedString(@"The resulting decrypted AR code doesn't contain any lines.") preferredStyle:UIAlertControllerStyleAlert];
    [empty_error addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:empty_error animated:true completion:nil];
    
    return;
  }
  
  if (!self.m_gecko_code->user_defined)
  {
    self.m_gecko_code = new Gecko::GeckoCode;
    self.m_should_be_pushed_back = true;
  }

  self.m_gecko_code->name = FoundationToCppString(self.m_name_field.text);
  self.m_gecko_code->creator = FoundationToCppString(self.m_creator_field.text);
  self.m_gecko_code->codes = std::move(entries);
  self.m_gecko_code->user_defined = true;
  
  std::vector<std::string> note_lines;
  NSArray<NSString*>* notes_array = [self.m_notes_view.text componentsSeparatedByString:@"\n"];
  
  for (NSString* str in notes_array)
  {
    note_lines.push_back(FoundationToCppString(str));
  }
  
  self.m_gecko_code->notes = std::move(note_lines);
  
  [self performSegueWithIdentifier:@"unwind_to_gecko" sender:nil];
}

@end
