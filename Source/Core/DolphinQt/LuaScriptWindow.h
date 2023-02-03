#pragma once

#include <QDialog>
#include <string>
#include <vector>

#include "Core/Lua/Lua.h"

class QDialogButtonBox;
class QListWidget;
class NonDefaultQPushButton;

class LuaScriptWindow : public QDialog
{
  Q_OBJECT
public:
  explicit LuaScriptWindow(QWidget* parent = nullptr);
  ~LuaScriptWindow();
  void LoadScriptFunction();
  void PlayScriptFunction();
  void StopScriptFunction();

private:
  void CreateMainLayout();
  void ConnectWidgets();
  std::function<void(const std::string&)> callbackPrintFunction;
  std::function<void()> finishedScriptCallbackFunction;
  void updateOutputWindow();
  void onScriptFinish();
  QListWidget* lua_output_list_widget_ptr;
  QListWidget* lua_script_name_list_widget_ptr;
  NonDefaultQPushButton* m_load_script_button;
  NonDefaultQPushButton* m_play_script_button;
  NonDefaultQPushButton* m_stop_script_button;
  std::string pathOfScriptToRun;
  std::vector<std::string> outputLines;



};
