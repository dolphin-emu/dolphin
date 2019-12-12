// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

class TCView: UIView
{
  var real_view: UIView?
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
    
    // Load ourselves from the nib
    let name = String(describing: type(of: self))
    let view = Bundle(for: type(of: self)).loadNibNamed(name, owner: self, options: nil)![0] as! UIView
    view.frame = self.bounds
    self.addSubview(view)
    
    real_view = view;
  }
  
}
