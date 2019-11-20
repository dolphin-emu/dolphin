// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@IBDesignable
class TCButton: UIButton
{
  let buttonMapping: [String: Int32] =
  [
    "gcpad_a": 0,
    "gcpad_b": 1,
    "gcpad_start": 2,
    "gcpad_x": 3,
    "gcpad_y": 4,
    "gcpad_z": 5,
    "wiimote_a": 100,
    "wiimote_b": 101,
    "wiimote_minus": 102,
    "wiimote_plus": 103
  ]
  
  @IBInspectable var m_type: String = ""
  {
    didSet
    {
      updateImage()
    }
  }
  
  override init(frame: CGRect)
  {
    super.init(frame: frame)
    sharedInit()
  }
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
    sharedInit()
  }
  
  override func prepareForInterfaceBuilder()
  {
    super.prepareForInterfaceBuilder()
    sharedInit()
  }
  
  func sharedInit()
  {
    self.setTitle("", for: .normal)
    self.addTarget(self, action: #selector(buttonPressed), for: .touchDown)
    self.addTarget(self, action: #selector(buttonReleased), for: .touchUpInside)
  }
  
  func updateImage()
  {
    let buttonImage = getImage(named: m_type)
    self.setImage(buttonImage?.withRenderingMode(.alwaysOriginal), for: .normal)
    self.setImage(getImage(named: m_type + "_pressed")?.withRenderingMode(.alwaysOriginal), for: .selected)
    self.frame = CGRect(origin: self.frame.origin, size: buttonImage?.size ?? CGSize(width: 192, height: 192))
  }
  
  func getImage(named: String) -> UIImage?
  {
    return UIImage(named: named, in: Bundle(for: type(of: self)), compatibleWith: nil)
  }
  
  @objc func buttonPressed()
  {
    MainiOS.gamepadEvent(forButton: buttonMapping[m_type] ?? 0, action: 1)
  }
  
  @objc func buttonReleased()
  {
    MainiOS.gamepadEvent(forButton: buttonMapping[m_type] ?? 0, action: 0)
  }
  
}
