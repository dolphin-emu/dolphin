// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import UIKit

class InGameConfigViewController: UITableViewController
{
  var m_emulation_controller: EmulationViewController? = nil
    
  @IBOutlet weak var haptic_feedback_switch: UISwitch!
  @IBOutlet weak var pad_opacity_slider: UISlider!
  @IBOutlet weak var pad_opacity_label: UILabel!
    
  override func viewDidLoad()
  {
    super.viewDidLoad()
    
    getSwitchState()
    // Set current opacity slider value
    let currentOpacity = UserDefaults.standard.float(forKey: "pad_opacity_value")
    let opacityString = String(currentOpacity).prefix(4)
    pad_opacity_slider.value = currentOpacity
    pad_opacity_label.text = String(opacityString)
    
    self.m_emulation_controller = self.parent?.presentingViewController?.children[0] as? EmulationViewController
    
    haptic_feedback_switch.addTarget(self, action: #selector(hapticFeedbackSwitchToggled(hapticSwitch:)), for: .valueChanged)
    pad_opacity_slider.addTarget(self, action: #selector(padOpacitySliderChanged(opacitySlider:)), for: .valueChanged)
  }
  
  override func tableView(_ tableView: UITableView, willSelectRowAt indexPath: IndexPath) -> IndexPath?
  {
    if (indexPath.section == 0 && indexPath.row == 1 && !m_emulation_controller!.isWii)
    {
      let alert = UIAlertController(title: "GameCube Game", message: "You cannot do this in a GameCube game.", preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
      self.present(alert, animated: true, completion: nil)
      
      return nil
    }
    
    return indexPath
  }
  
  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath)
  {
    if (indexPath.section == 0 && indexPath.row == 0)
    {
      if (m_emulation_controller!.isWii)
      {
        self.performSegue(withIdentifier: "controller_toggle_wii", sender: nil)
      }
      else
      {
        self.performSegue(withIdentifier: "controller_toggle_gc", sender: nil)
      }
    }
  }
  
  override func prepare(for segue: UIStoryboardSegue, sender: Any?)
  {
    if (segue.identifier?.starts(with: "controller_toggle") ?? false)
    {
      let destination = segue.destination as! ControllerToggleViewController
      destination.m_emulation_controller = self.m_emulation_controller
    }
  }
    
  func getSwitchState()
  {
    // If user is on an iPad, disable use of the switch
    if UIDevice.current.userInterfaceIdiom == .pad
    {
        haptic_feedback_switch.setOn(false, animated: false)
        haptic_feedback_switch.isEnabled = false
    }
    else
    {
        // Created in case more switches are added to this view so viewdidload isn't cluttered
        haptic_feedback_switch.setOn(UserDefaults.standard.bool(forKey: "haptic_feedback_enabled"), animated: false)
    }
  }
    
  @objc func hapticFeedbackSwitchToggled(hapticSwitch: UISwitch)
  {
    UserDefaults.standard.set(hapticSwitch.isOn, forKey: "haptic_feedback_enabled")
  }
    
  @objc func padOpacitySliderChanged(opacitySlider: UISlider)
  {
    let outputString = String(opacitySlider.value).prefix(4)
    pad_opacity_label.text = String(outputString)
    
    UserDefaults.standard.set(opacitySlider.value, forKey: "pad_opacity_value")
    m_emulation_controller?.setPadOpacity()
  }
}
