// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import UIKit

class ControllerToggleViewController: UITableViewController
{
  var m_emulation_controller: EmulationViewController? = nil
  
  override func viewDidAppear(_ animated: Bool)
  {
    super.viewDidAppear(animated)
    
    // Set the current controller as checked
    let controller = self.m_emulation_controller!.m_visible_controller
    self.tableView.cellForRow(at: IndexPath(row: controller, section: 0))?.accessoryType = .checkmark
  }
  
  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath)
  {
    // Get the current controller
    let controller = self.m_emulation_controller!.m_visible_controller
    self.tableView.cellForRow(at: IndexPath(row: controller, section: 0))?.accessoryType = .none
    
    // Change the controller
    self.m_emulation_controller!.changeController(idx: indexPath.row)
    self.tableView.cellForRow(at: indexPath)?.accessoryType = .checkmark
    
    if (self.m_emulation_controller!.isWii)
    {
      MainiOS.toggleSidewaysWiimote(indexPath.row == 1, reload_wiimote: true)
    }
    
    self.tableView.deselectRow(at: indexPath, animated: true)
  }
  
}
