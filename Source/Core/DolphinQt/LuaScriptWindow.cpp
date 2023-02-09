#include "LuaScriptWindow.h"
#include <QDialogButtonBox>
#include <QListWidget>
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

LuaScriptWindow::LuaScriptWindow(QWidget* parent) : QDialog(parent)
{
  callback_print_function = [this](const std::string& message) {
    output_lines.push_back(message);
    QueueOnObject(this, &LuaScriptWindow::UpdateOutputWindow);
  };

  finished_script_callback_function = [this]() {
    QueueOnObject(this, &LuaScriptWindow::OnScriptFinish);
  };

  CreateMainLayout();
  ConnectWidgets();
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("luascriptwindow/geometry")).toByteArray());
}

LuaScriptWindow::~LuaScriptWindow()
{
  Lua::is_lua_script_active = false;
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("luascriptwindow/geometry"), saveGeometry());
}

void LuaScriptWindow::CreateMainLayout()
{
  auto* layout = new QGridLayout();
  m_load_script_button = new NonDefaultQPushButton(tr("Load Script"));

  m_play_script_button = new NonDefaultQPushButton(tr("Play Script"));
  m_play_script_button->setStyleSheet(tr("background-color:grey;"));
  m_stop_script_button = new NonDefaultQPushButton(tr("Stop Script"));
  m_stop_script_button->setStyleSheet(tr("background-color:grey;"));

  lua_script_name_list_widget_ptr = new QListWidget;
  lua_script_name_list_widget_ptr->setMaximumHeight(100);

  QLabel* script_name_header = new QLabel(tr("Script Name:"));
  auto* script_name_box = new QGridLayout();
  script_name_box->addWidget(script_name_header, 0, 0, Qt::AlignTop);
  script_name_box->addWidget(lua_script_name_list_widget_ptr, 1, 0, Qt::AlignTop);

  QLabel* output_header = new QLabel(tr("Output:"));
  lua_output_list_widget_ptr = new QListWidget;
  auto* output_box = new QGridLayout();
  output_box->addWidget(output_header, 0, 0, Qt::AlignTop);
  output_box->addWidget(lua_output_list_widget_ptr, 1, 0, Qt::AlignTop);

  lua_output_list_widget_ptr->setSpacing(1);
  lua_output_list_widget_ptr->setStyleSheet(tr("background-color:white;"));

  layout->addWidget(m_load_script_button, 0, 0, Qt::AlignTop);
  layout->addWidget(m_play_script_button, 0, 1, Qt::AlignTop);
  layout->addWidget(m_stop_script_button, 0, 2, Qt::AlignTop);
  layout->addLayout(script_name_box, 1, 0, Qt::AlignTop);
  layout->addLayout(output_box, 1, 1, Qt::AlignTop);
  setWindowTitle(tr("Lua Script Manager"));
  setLayout(layout);
}

void LuaScriptWindow::ConnectWidgets()
{
  connect(m_load_script_button, &QPushButton::clicked, this, &LuaScriptWindow::LoadScriptFunction);
  connect(m_play_script_button, &QPushButton::clicked, this, &LuaScriptWindow::PlayScriptFunction);
  connect(m_stop_script_button, &QPushButton::clicked, this, &LuaScriptWindow::StopScriptFunction);
}

void LuaScriptWindow::LoadScriptFunction()
{
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getOpenFileName(
      this, tr("Select a File"),
      settings.value(QStringLiteral("luascriptwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.lua *.txt);;%2 (*)")
          .arg(tr("All lua/txt files"))
          .arg(tr("All Files")));

  if (!path.isEmpty())
  {
    settings.setValue(QStringLiteral("luascriptwindow/lastdir"), QFileInfo(path).absoluteDir().absolutePath());
    path_of_script_to_run = path.toStdString();
    QStringList list;
    list << path;
    lua_script_name_list_widget_ptr->clear();
    lua_script_name_list_widget_ptr->insertItems(0, list);
    m_play_script_button->setStyleSheet(tr("background-color:green;"));
  }

  return;
}

void LuaScriptWindow::PlayScriptFunction()
{
  if (Lua::is_lua_script_active || path_of_script_to_run.empty())
    return;

  lua_output_list_widget_ptr->clear();
  Lua::Init(path_of_script_to_run, &callback_print_function, &finished_script_callback_function);
  m_play_script_button->setStyleSheet(tr("background-color:grey;"));
  m_stop_script_button->setStyleSheet(tr("background-color:red;"));
}

void LuaScriptWindow::StopScriptFunction()
{
  if (!Lua::is_lua_core_initialized || !Lua::is_lua_script_active)
    return;

  Lua::StopScript();
  m_play_script_button->setStyleSheet(tr("background-color:green;"));
  m_stop_script_button->setStyleSheet(tr("background-color:grey;"));
}

void LuaScriptWindow::UpdateOutputWindow()
{
  QStringList list;
  size_t output_size = output_lines.size();
  for (size_t i = 0; i < output_size; ++i)
    list << QString::fromStdString(output_lines[i]);
  lua_output_list_widget_ptr->clear();
  lua_output_list_widget_ptr->insertItems(0, list);
  lua_output_list_widget_ptr->setSpacing(1);
}

void LuaScriptWindow::OnScriptFinish()
{
  m_play_script_button->setStyleSheet(tr("background-color:green;"));
  m_stop_script_button->setStyleSheet(tr("background-color:grey;"));
}
