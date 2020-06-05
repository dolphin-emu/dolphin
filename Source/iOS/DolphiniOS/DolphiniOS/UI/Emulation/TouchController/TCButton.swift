// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@IBDesignable
class TCButton: UIButton
{
  let hapticGenerator = UIImpactFeedbackGenerator(style: .medium)
    
  @IBInspectable var controllerButton: Int = 0 // default: GC A button
  {
    didSet
    {
      updateImage()
    }
  }
  
  @IBInspectable var isAxis: Bool = false
  
  var m_port: Int = 0
  
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
    let buttonType = TCButtonType(rawValue: controllerButton)!
    
    let buttonImage = getImage(named: buttonType.getImageName(), scale: buttonType.getButtonScale())
    self.setImage(buttonImage, for: .normal)
    
    let buttonPressedImage = getImage(named: buttonType.getImageName() + "_pressed", scale: buttonType.getButtonScale())
    self.setImage(buttonPressedImage, for: .selected)
  }
  
  func getImage(named: String, scale: CGFloat) -> UIImage
  {
    // In Interface Builder, the default bundle is not Dolphin's, so we must specify
    // the bundle for the image to load correctly
    let image = UIImage(named: named, in: Bundle(for: type(of: self)), compatibleWith: nil)!
    
    // Create a new CGSize with the new scale
    let newSize = CGSize(width: image.size.width * scale, height: image.size.height * scale)
    
    // Render the image into a context
    UIGraphicsBeginImageContext(newSize)
    image.draw(in: CGRect(origin: CGPoint.zero, size: newSize))
    let newImage = UIGraphicsGetImageFromCurrentImageContext()!
    UIGraphicsEndImageContext()
    
    return newImage.withRenderingMode(.alwaysOriginal)
  }
  
  @objc func buttonPressed()
  {
    // Check UserDefaults for haptic setting
    if (UserDefaults.standard.bool(forKey: "haptic_feedback_enabled"))
    {
        hapticGenerator.impactOccurred()
    }
    
    if (isAxis)
    {
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(controllerButton), value: 1.0)
    }
    else
    {
      MainiOS.gamepadEvent(onPad: Int32(self.m_port), button: Int32(controllerButton), action: 1)
    }
  }
  
  @objc func buttonReleased()
  {
    if (isAxis)
    {
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(controllerButton), value: 0)
    }
    else
    {
      MainiOS.gamepadEvent(onPad: Int32(self.m_port), button: Int32(controllerButton), action: 0)
    }
  }
  
}
