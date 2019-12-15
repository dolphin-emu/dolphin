// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import MetalKit
import UIKit

class EmulationViewController: UIViewController
{
  @objc public var softwareFile: String = ""
  @objc public var softwareName: String = ""
  @objc public var isWii: Bool = false
  
  @IBOutlet weak var m_metal_view: MTKView!
  @IBOutlet weak var m_eagl_view: EAGLView!
  @IBOutlet weak var m_gc_pad_view: TCGameCubePad!
  @IBOutlet weak var m_wii_pad_view: TCWiiPad!
  
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
    
    if (self.isWii)
    {
      m_wii_pad_view.isUserInteractionEnabled = true
      m_wii_pad_view.isHidden = false
    }
    else
    {
      m_gc_pad_view.isUserInteractionEnabled = true
      m_gc_pad_view.isHidden = false
    }
    
    let has_seen_alert = UserDefaults.standard.bool(forKey: "seen_double_tap_alert")
    if (!has_seen_alert)
    {
      let alert = UIAlertController(title: "Note", message: "Double tap the screen to reveal the top bar.", preferredStyle: .alert)
      
      alert.addAction(UIAlertAction(title: "OK", style: .default, handler: { action in
        self.navigationController!.setNavigationBarHidden(true, animated: true)
        
        UserDefaults.standard.set(true, forKey: "seen_double_tap_alert")
      }))
      
      self.present(alert, animated: true, completion: nil)
    }
    else
    {
      self.navigationController!.setNavigationBarHidden(true, animated: true)
    }
    
    let wiimote_queue = DispatchQueue(label: "org.dolphin-emu.ios.wiimote-initial-queue")
    wiimote_queue.async
    {
      // Wait for aspect ratio to be set
      while (MainiOS.getGameAspectRatio() == 0)
      {
      }
      
      // Create the Wiimote pointer values
      DispatchQueue.main.sync
      {
        self.m_wii_pad_view.recalculatePointerValues()
      }
    }
    
    let queue = DispatchQueue(label: "org.dolphin-emu.ios.emulation-queue")
    queue.async
    {
      MainiOS.startEmulation(withFile: self.softwareFile, view: renderer_view)
      
      DispatchQueue.main.async {
        self.performSegue(withIdentifier: "toSoftwareTable", sender: nil)
      }
    }
  }
  
  @IBAction func tapInController(_ sender: UITapGestureRecognizer)
  {
    // Ignore double taps on things that can be tapped
    if (sender.view != nil)
    {
      let view = (sender.view as! TCView).real_view
      let hit_view = view?.hitTest(sender.location(in: view), with: nil)
      if (hit_view != view)
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
  
  override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator)
  {
    super.viewWillTransition(to: size, with: coordinator)
    
    // Perform an "animation" alongside the transition and tell Dolphin that
    // the window has resized after it is finished
    coordinator.animate(alongsideTransition: nil, completion: { _ in
      MainiOS.windowResized()
      self.m_wii_pad_view.recalculatePointerValues()
    })
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
  
}
