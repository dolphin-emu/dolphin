// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import MetalKit
import UIKit

class EmulationViewController: UIViewController, UIGestureRecognizerDelegate
{
  @objc public var softwareFile: String = ""
  @objc public var softwareName: String = ""
  @objc public var isWii: Bool = false
  
  public var m_visible_controller = 0
  
  let setup_queue = DispatchQueue(label: "org.dolphin-emu.ios.setup-queue")
  
  @IBOutlet weak var m_metal_view: MTKView!
  @IBOutlet weak var m_eagl_view: EAGLView!
  @IBOutlet weak var m_gc_pad_view: TCGameCubePad!
  @IBOutlet weak var m_wii_pad_view: TCWiiPad!
  @IBOutlet weak var m_wii_pad_sideways_view: TCSidewaysWiiPad!
  
  @IBOutlet var m_gc_controllers: [UIView]!
  @IBOutlet var m_wii_controllers: [UIView]!
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
  }
  
  override func viewDidLoad()
  {
    self.navigationItem.title = self.softwareName;
    
    var renderer_view: UIView
    if (MainiOS.getGfxBackend() == "Vulkan")
    {
      renderer_view = m_metal_view
    }
    else
    {
      renderer_view = m_eagl_view
    }
    
    renderer_view.isHidden = false
    
    self.changeController(idx: 0)
    
    setupTapGestureRecognizer(m_wii_pad_view)
    setupTapGestureRecognizer(m_wii_pad_sideways_view)
    setupTapGestureRecognizer(m_gc_pad_view)
    
    let has_seen_initial_alert = UserDefaults.standard.bool(forKey: "seen_double_tap_two_fingers_alert")
    let has_seen_options_alert = UserDefaults.standard.bool(forKey: "seen_new_options_menu_alert")
    if (!has_seen_initial_alert)
    {
      let alert = UIAlertController(title: "Note", message: "Double tap the screen with two fingers fast to reveal the top bar.\n\nYou can adjust some emulation settings by pressing the button on the left of the top bar.", preferredStyle: .alert)
      
      alert.addAction(UIAlertAction(title: "OK", style: .default, handler: { action in
        self.navigationController!.setNavigationBarHidden(true, animated: true)
        
        UserDefaults.standard.set(true, forKey: "seen_double_tap_two_fingers_alert")
        UserDefaults.standard.set(true, forKey: "seen_new_options_menu_alert")
      }))
      
      self.present(alert, animated: true, completion: nil)
    }
    else if (!has_seen_options_alert)
    {
      let alert = UIAlertController(title: "Note", message: "There is now an options menu on the left side of the top bar. You can toggle motion controls there.\n\nFor games that use the Wii Motion Plus accessory, you should select the motion control mode \"On (With Pointer Emulation)\".", preferredStyle: .alert)
      
      alert.addAction(UIAlertAction(title: "OK", style: .default, handler: { action in
        self.navigationController!.setNavigationBarHidden(true, animated: true)
        
        UserDefaults.standard.set(true, forKey: "seen_new_options_menu_alert")
      }))
      
      self.present(alert, animated: true, completion: nil)
    }
    else
    {
      self.navigationController!.setNavigationBarHidden(true, animated: true)
    }
    
    if (self.isWii)
    {
      TCDeviceMotion.shared.registerMotionHandlers()
    }
    
    let queue = DispatchQueue(label: "org.dolphin-emu.ios.emulation-queue")
    queue.async
    {
      MainiOS.toggleSidewaysWiimote(false, reload_wiimote: false)
      
      MainiOS.startEmulation(withFile: self.softwareFile, viewController: self, view: renderer_view)
      
      if (self.isWii)
      {
        TCDeviceMotion.shared.stopMotionUpdates()
      }
      
      DispatchQueue.main.async {
        self.performSegue(withIdentifier: "toSoftwareTable", sender: nil)
      }
    }
  }
  
  override func viewDidLayoutSubviews()
  {
    if (self.isWii)
    {
      TCDeviceMotion.shared.statusBarOrientationChanged()
    }
    
    self.adjustRenderRectDependencies()
    MainiOS.windowResized()
  }
  
  override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator)
  {
    super.viewWillTransition(to: size, with: coordinator)
    
    // Perform an "animation" alongside the transition and tell Dolphin that
    // the window has resized after it is finished
    /*coordinator.animate(alongsideTransition: nil, completion: { _ in
      self.adjustRenderRectDependencies()
      MainiOS.windowResized()
    })*/
  }
  
  func adjustRenderRectDependencies()
  {
    setup_queue.async
    {
      let target_rectangle = MainiOS.getRenderTargetRectangle()
      var new_rect: CGRect
      
      // Wait for the target rectangle to be set
      while (true)
      {
        new_rect = MainiOS.getRenderTargetRectangle()
        if (new_rect.size != target_rectangle.size)
        {
          break;
        }
      }
      
      DispatchQueue.main.sync
      {
        // Only adjust the render rectangle if our width size class is compact and our
        // orientation is portrait
        if (self.traitCollection.horizontalSizeClass == .compact && self.traitCollection.verticalSizeClass == .regular &&  UIApplication.shared.statusBarOrientation.isPortrait)
        {
          // Get view information
          let screen_scale = UIScreen.main.scale
          let view_origin = self.view.bounds.origin
          
          // Get the X coordinate
          let x = view_origin.x * screen_scale
          
          // Get the Y coordinate as the navigation bar height plus an offset
          let y_offset: CGFloat = 20.0
          let y = (self.navigationController!.navigationBar.bounds.height + y_offset) * screen_scale
          
          MainiOS.setDrawRectangleCustomOriginAsX(Int32(x), y: Int32(y))
        }
        else
        {
          // Don't adjust the rectangle
          MainiOS.setDrawRectangleCustomOriginAsX(0, y: 0)
        }
        
        for view in self.m_wii_controllers
        {
          let wii_pad = view as? TCWiiPad
          if (wii_pad != nil)
          {
            wii_pad!.recalculatePointerValues()
          }
        }
      }
    }
  }
  
  func setupTapGestureRecognizer(_ view: TCView)
  {
    // Add a gesture recognizer for two finger double tapping
    let tap_recognizer = UITapGestureRecognizer(target: self, action: #selector(doubleTapped))
    tap_recognizer.numberOfTapsRequired = 2
    tap_recognizer.numberOfTouchesRequired = 2
    tap_recognizer.delegate = self
    
    view.real_view!.addGestureRecognizer(tap_recognizer)
  }
  
  @IBAction func doubleTapped(_ sender: UITapGestureRecognizer)
  {
    // Ignore double taps on things that can be tapped
    if (sender.view != nil)
    {
      let hit_view = sender.view!.hitTest(sender.location(in: sender.view), with: nil)
      if (hit_view != sender.view)
      {
        return
      }
    }
    
    let is_hidden = self.navigationController!.isNavigationBarHidden
    if (!is_hidden)
    {
      self.additionalSafeAreaInsets.top = 0
    }
    else
    {
      // This inset undoes any changes the navigation bar made to the safe area
      self.additionalSafeAreaInsets.top = -(self.navigationController!.navigationBar.bounds.height)
    }
    
    self.navigationController!.setNavigationBarHidden(!is_hidden, animated: true)
    
  }
  
  func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRequireFailureOf otherGestureRecognizer: UIGestureRecognizer) -> Bool {
    if (gestureRecognizer is UITapGestureRecognizer && otherGestureRecognizer is UILongPressGestureRecognizer)
    {
      return true
    }
    
    return false
  }
  
  func changeController(idx: Int)
  {
    if (self.isWii)
    {
      for i in 0...self.m_wii_controllers.count - 1
      {
        if (i == idx)
        {
          self.m_wii_controllers[i].isUserInteractionEnabled = true
          self.m_wii_controllers[i].isHidden = false
        }
        else
        {
          self.m_wii_controllers[i].isUserInteractionEnabled = false
          self.m_wii_controllers[i].isHidden = true
        }
      }
    }
    else
    {
      for i in 0...self.m_gc_controllers.count - 1
      {
        if (i == idx)
        {
          self.m_gc_controllers[i].isUserInteractionEnabled = true
          self.m_gc_controllers[i].isHidden = false
        }
        else
        {
          self.m_gc_controllers[i].isUserInteractionEnabled = false
          self.m_gc_controllers[i].isHidden = true
        }
      }
    }
    
    self.m_visible_controller = idx
  }
  
  @IBAction func exitButtonPressed(_ sender: Any)
  {
    let alert = UIAlertController(title: "Stop Emulation", message: "Do you really want to stop the emulation? All unsaved data will be lost.", preferredStyle: .alert)
    
    alert.addAction(UIAlertAction(title: "No", style: .default, handler: nil))
    alert.addAction(UIAlertAction(title: "Yes", style: .destructive, handler: { action in
      MainiOS.stopEmulation()
    }))
    
    self.present(alert, animated: true, completion: nil)
  }
  
  @IBAction func unwindToEmulationView(segue: UIStoryboardSegue) {}
  
}
