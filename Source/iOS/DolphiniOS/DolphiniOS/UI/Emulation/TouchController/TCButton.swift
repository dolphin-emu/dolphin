// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@IBDesignable
class TCButton: UIButton
{
  @IBInspectable var controllerButton: Int = 0 // default: GC A button
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
    let buttonImage = getImage(named: TCButtonType(rawValue: controllerButton)!.getImageName())
    self.setImage(buttonImage, for: .normal)
    
    let buttonPressedImage = getImage(named: TCButtonType(rawValue: controllerButton)!.getImageName() + "_pressed")
    self.setImage(buttonPressedImage, for: .selected)
    
    self.frame = CGRect(origin: self.frame.origin, size: buttonImage.size)
  }
  
  func getImage(named: String) -> UIImage
  {
    // In Interface Builder, the default bundle is not Dolphin's, so we must specify
    // the bundle for the image to load correctly
    return UIImage(named: named, in: Bundle(for: type(of: self)), compatibleWith: nil)!.withRenderingMode(.alwaysOriginal)
  }
  
  @objc func buttonPressed()
  {
    MainiOS.gamepadEvent(forButton: Int32(controllerButton), action: 1)
  }
  
  @objc func buttonReleased()
  {
    MainiOS.gamepadEvent(forButton: Int32(controllerButton), action: 0)
  }
  
}
