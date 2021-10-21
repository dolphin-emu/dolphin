// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <Cocoa/Cocoa.h>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

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
