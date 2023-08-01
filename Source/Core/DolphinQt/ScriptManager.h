#pragma once

#include <mutex>

#include <QDialog>
#include <string>
#include <vector>

#include "Common/SPSCQueue.h"
#include "Core/Scripting/ScriptUtilities.h"

class QDialogButtonBox;
class QListWidget;
class NonDefaultQPushButton;

class ScriptManager : public QDialog
{
  Q_OBJECT
public:
  explicit ScriptManager(QWidget* parent = nullptr);
  ~ScriptManager();
  void LoadScriptFunction();
  void PlayScriptFunction();
  void StopScriptFunction();
  void PlayOrStopScriptFunction();

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void UpdateOutputWindow();
  void OnScriptFinish();
  void UpdateButtonText();

  QListWidget* script_output_list_widget_ptr;
  QListWidget* script_name_list_widget_ptr;
  NonDefaultQPushButton* m_load_script_button;
  NonDefaultQPushButton* m_play_or_stop_script_button;
  std::vector<std::string> output_lines;
  Common::SPSCQueue<int, false> ids_of_scripts_to_stop;
  std::map<int, bool> row_num_to_is_running;  // row_num = unique_identifier for script
  std::mutex print_lock;
  std::mutex script_start_or_stop_lock;
  int next_unique_identifier;
};
