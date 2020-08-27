// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Cocoa/Cocoa.h>

int main(int argc, const char** argv)
{
  if (argc == 1)
  {
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];

    [alert setMessageText:@"This updater is not meant to be launched directly."];
    [alert setAlertStyle:NSAlertStyleWarning];
    [alert setInformativeText:@"Configure Auto-Update in Dolphin's settings instead."];
    [alert runModal];

    return 1;
  }

  return NSApplicationMain(argc, argv);
}
