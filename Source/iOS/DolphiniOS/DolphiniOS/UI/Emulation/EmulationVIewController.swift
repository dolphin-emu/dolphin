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
    
    let queue = DispatchQueue(label: "org.dolphin-emu.ios.emulation-queue")
    queue.async
    {
      MainiOS.startEmulation(withFile: self.softwareFile, view: self.m_metal_view)
      
      DispatchQueue.main.async {
        self.performSegue(withIdentifier: "toSoftwareTable", sender: nil)
      }
    }
  }
  
  @IBAction func tapInController(_ sender: UITapGestureRecognizer)
  {
    // Ignore double taps on things that can be tapped
    let hit_view = sender.view?.window?.hitTest(sender.location(in: nil), with: nil)
    if (hit_view is TCButton || hit_view is TCJoystick || hit_view is TCDirectionalPad)
    {
      return
    }
    
    self.navigationController!.setNavigationBarHidden(!self.navigationController!.isNavigationBarHidden, animated: true)
  }
  
  override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator)
  {
    MainiOS.windowResized()
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
