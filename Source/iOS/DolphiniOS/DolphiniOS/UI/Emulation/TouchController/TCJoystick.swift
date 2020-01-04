// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@IBDesignable
class TCJoystick: UIView
{
  @IBInspectable var joystickType: Int = 10 // default: GC stick
  var m_port: Int = 0
  
  override init(frame: CGRect)
  {
    super.init(frame: frame)
  }
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
  }
  
  override func awakeFromNib()
  {
    super.awakeFromNib()
    sharedInit()
  }
  
  override func prepareForInterfaceBuilder()
  {
    super.prepareForInterfaceBuilder()
    sharedInit()
  }
  
  func sharedInit()
  {
    // Create the range
    let rangeImage = createImageView(imageName: "gcwii_joystick_range")
    self.addSubview(rangeImage)
    
    // Create handle
    let handleView = createImageView(imageName: TCButtonType(rawValue: joystickType)!.getImageName())
    let panHandler = UIPanGestureRecognizer(target: self, action: #selector(handlePan))
    handleView.isUserInteractionEnabled = true
    handleView.addGestureRecognizer(panHandler)
    self.addSubview(handleView)
    
    // Set background color to transparent
    self.backgroundColor = UIColor.clear
  }
  
  func createImageView(imageName: String) -> UIImageView
  {
    // In Interface Builder, the default bundle is not Dolphin's, so we must specify
    // the bundle for the image to load correctly
    let image = UIImage(named: imageName, in: Bundle(for: type(of: self)), compatibleWith: nil)
    
    // Create the view
    let imageView = UIImageView(frame: CGRect(x: 0, y: 0, width: self.frame.width - (self.frame.width / 3), height: self.frame.height - (self.frame.height / 3)))
    imageView.image = image
    imageView.center = self.convert(self.center, from: self.superview)
    
    return imageView
  }
  
  @objc func handlePan(gesture: UIPanGestureRecognizer)
  {
    var point: CGPoint
    var joyAxises: [CGFloat] = [ 0, 0, 0, 0 ]
    
    if (gesture.state == .ended)
    {
      // Reset to center
      point = self.convert(self.center, from: self.superview)
    }
    else
    {
      // Get points
      point = gesture.location(in: self)
      let joystickCenter = self.convert(self.center, from: self.superview)
      
      // Calculate differences
      let xDiff = point.x - joystickCenter.x
      let yDiff = point.y - joystickCenter.y
      
      // Calculate distance
      let distance = sqrt(pow(xDiff, 2) + pow(yDiff, 2))
      let maxDistance = self.frame.width / 3
      
      if (distance > maxDistance)
      {
        // Calculate maximum points
        let xMax = joystickCenter.x + maxDistance * (xDiff / distance)
        let yMax = joystickCenter.y + maxDistance * (yDiff / distance)
        
        point = CGPoint(x: xMax, y: yMax)
      }
      
      // Calculate axis values for ButtonManager
      // Based on Android's getAxisValues()
      let axises = [ yDiff / maxDistance, xDiff / maxDistance ]
      joyAxises = [ min(axises[0], 0), min(axises[0], 1), min(axises[1], 0), min(axises[1], 1)]
    }
    
    // Send axises values
    let axisStartIdx = joystickType
    for i in 0...3
    {
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(axisStartIdx + i + 1), value: joyAxises[i])
    }
    
    gesture.view?.center = point
  }
  
}
