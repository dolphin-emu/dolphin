// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

@IBDesignable
class TCDirectionalPad: UIView
{
  let hapticGenerator = UIImpactFeedbackGenerator(style: .medium)

  var dpadNoPressed: UIImage? = nil
  var dpadOnePressed: UIImage? = nil
  var dpadTwoPressed: UIImage? = nil
  
  @IBInspectable var directionalPadType: Int = 6 // default: GC D-Pad
  
  var m_port: Int = 0
  var m_has_pressed: Bool = false
  
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
    // Load image
    dpadNoPressed = getImage(imageName: "gcwii_dpad")
    dpadOnePressed = getImage(imageName: "gcwii_dpad_pressed_one_direction")
    dpadTwoPressed = getImage(imageName: "gcwii_dpad_pressed_two_directions")
    
    // Create the image view
    let imageView = UIImageView(frame: CGRect(x: 0, y: 0, width: self.frame.width, height: self.frame.height))
    imageView.image = dpadNoPressed
    imageView.center = self.convert(self.center, from: self.superview)
    imageView.isUserInteractionEnabled = true
      
    let pressHandler = UILongPressGestureRecognizer(target: self, action: #selector(handleLongPress))
    pressHandler.minimumPressDuration = 0
    imageView.addGestureRecognizer(pressHandler)
    
    self.addSubview(imageView)
    
    // Set background color to transparent
    self.backgroundColor = UIColor.clear
  }
  
  func getImage(imageName: String) -> UIImage
  {
    // In Interface Builder, the default bundle is not Dolphin's, so we must specify
    // the bundle for the image to load correctly
    return UIImage(named: imageName, in: Bundle(for: type(of: self)), compatibleWith: nil)!
  }
  
  @objc func handleLongPress(gesture: UILongPressGestureRecognizer)
  {
    let imageView = gesture.view as! UIImageView
    let point = gesture.location(in: self)
    var buttonPresses: [Bool] = [ false, false, false, false ]
    
    if (gesture.state == .ended)
    {
      imageView.image = dpadNoPressed
      self.m_has_pressed = false
    }
    else
    {
      // Check UserDefaults for haptic setting
      if (!self.m_has_pressed && UserDefaults.standard.bool(forKey: "haptic_feedback_enabled"))
      {
        hapticGenerator.impactOccurred()
        self.m_has_pressed = true
      }
      
      // Get button boundary
      let buttonBounds = (gesture.view?.frame.width)! / 3
      
      // Up
      if (point.y <= buttonBounds)
      {
        buttonPresses[0] = true;
      }
      else if (point.y >= buttonBounds * 2) // Down
      {
        buttonPresses[1] = true;
      }
      
      // Left
      if (point.x <= buttonBounds)
      {
        buttonPresses[2] = true;
      }
      else if (point.x >= buttonBounds * 2) // Right
      {
        buttonPresses[3] = true;
      }
      
      var rotation: CGFloat = 0
      
      // TODO: is there a better way to structure this?
      // Left and Up
      if (buttonPresses[2] && buttonPresses[0])
      {
        imageView.image = dpadTwoPressed
        rotation = 0
      }
      else if (buttonPresses[0] && buttonPresses[3]) // Up and Right
      {
        imageView.image = dpadTwoPressed
        rotation = 90
      }
      else if (buttonPresses[3] && buttonPresses[1]) // Right and Down
      {
        imageView.image = dpadTwoPressed
        rotation = 180
      }
      else if (buttonPresses[1] && buttonPresses[2]) // Down and Left
      {
        imageView.image = dpadTwoPressed
        rotation = 270
      }
      else if (buttonPresses[0]) // Up
      {
        imageView.image = dpadOnePressed
        rotation = 0
      }
      else if (buttonPresses[1]) // Down
      {
        imageView.image = dpadOnePressed
        rotation = 180
      }
      else if (buttonPresses[2]) // Left
      {
        imageView.image = dpadOnePressed
        rotation = 270
      }
      else if (buttonPresses[3]) // Right
      {
        imageView.image = dpadOnePressed
        rotation = 90
      }
      else
      {
        imageView.image = dpadNoPressed
      }
      
      let radians = rotation * (CGFloat.pi / 180)
      imageView.transform = CGAffineTransform.identity.rotated(by: radians)
    }
    
    // Send button values
    for i in 0...3
    {
      MainiOS.gamepadEvent(onPad: Int32(self.m_port), button: Int32(directionalPadType + i), action: buttonPresses[i] ? 1 : 0)
    }
  }
  
}
