// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// https://stackoverflow.com/a/56583774
extension UINavigationController
{
  open override var childForHomeIndicatorAutoHidden: UIViewController?
  {
    return topViewController
  }
}
