// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@objc class TCView: UIView
{
  var real_view: UIView?
  
  private var _m_port: Int = 0
  @objc var m_port: Int
  {
    get
    {
      return _m_port
    }
    set
    {
      _m_port = newValue
      
      SetPort(newValue, view: real_view!)
    }
  }
  
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
  
  func SetPort(_ port: Int, view: UIView)
  {
    for subview in view.subviews
    {
      if (subview is TCButton)
      {
        (subview as! TCButton).m_port = port
      }
      else if (subview is TCJoystick)
      {
        (subview as! TCJoystick).m_port = port
      }
      else if (subview is TCDirectionalPad)
      {
        (subview as! TCDirectionalPad).m_port = port
      }
      else
      {
        SetPort(port, view: subview)
      }
    }
  }
  
}
