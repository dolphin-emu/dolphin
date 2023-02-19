#include "DolphinQt/LuaScriptWindow.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

LuaScriptWindow::LuaScriptWindow(QWidget* parent) : QDialog(parent)
{
  next_unique_identifier = 0;
  callback_print_function = [this](const std::string& message) {
    std::lock_guard<std::mutex> lock(print_lock);
    output_lines.push_back(message);
    QueueOnObject(this, &LuaScriptWindow::UpdateOutputWindow);
  };

  finished_script_callback_function = [this](int unique_identifier) {
    ids_of_scripts_to_stop.push(unique_identifier);
    QueueOnObject(this, &LuaScriptWindow::OnScriptFinish);
  };

  CreateMainLayout();
  ConnectWidgets();
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("luascriptwindow/geometry")).toByteArray());
}

LuaScriptWindow::~LuaScriptWindow()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("luascriptwindow/geometry"), saveGeometry());
}

void LuaScriptWindow::CreateMainLayout()
{
  auto* layout = new QGridLayout();
  m_load_script_button = new NonDefaultQPushButton(tr("Load Script"));

  m_play_or_stop_script_button = new NonDefaultQPushButton(tr("N/A"));
  m_play_or_stop_script_button->setStyleSheet(tr("background-color:grey;"));

  lua_script_name_list_widget_ptr = new QListWidget;
  lua_script_name_list_widget_ptr->setMinimumHeight(100);
  lua_script_name_list_widget_ptr->setMaximumHeight(200);
  lua_script_name_list_widget_ptr->setMinimumWidth(550);
  lua_script_name_list_widget_ptr->setMaximumWidth(800);

  QLabel* script_name_header = new QLabel(tr("Script Name:"));
  auto* script_name_box = new QGridLayout();
  script_name_box->addWidget(script_name_header, 0, 0, Qt::AlignTop);
  script_name_box->addWidget(lua_script_name_list_widget_ptr, 1, 0, Qt::AlignTop);

  QLabel* output_header = new QLabel(tr("Output:"));
  lua_output_list_widget_ptr = new QListWidget;
  lua_output_list_widget_ptr->setMinimumHeight(300);
  lua_output_list_widget_ptr->setMaximumHeight(600);
  lua_output_list_widget_ptr->setMinimumWidth(550);
  lua_output_list_widget_ptr->setMaximumWidth(800);
  auto* output_box = new QGridLayout();
  output_box->addWidget(output_header, 0, 0, Qt::AlignTop);
  output_box->addWidget(lua_output_list_widget_ptr, 1, 0, Qt::AlignTop);

  lua_output_list_widget_ptr->setSpacing(1);
  lua_output_list_widget_ptr->setStyleSheet(tr("background-color:white;"));

  layout->addWidget(m_load_script_button, 0, 0, Qt::AlignTop);
  layout->addWidget(m_play_or_stop_script_button, 0, 1, Qt::AlignTop);
  layout->addLayout(script_name_box, 1, 0, Qt::AlignTop);
  layout->addLayout(output_box, 1, 1, Qt::AlignTop);
  setWindowTitle(tr("Lua Script Manager"));
  setLayout(layout);
}

void LuaScriptWindow::ConnectWidgets()
{
  connect(m_load_script_button, &QPushButton::clicked, this, &LuaScriptWindow::LoadScriptFunction);
  connect(m_play_or_stop_script_button, &QPushButton::clicked, this, &LuaScriptWindow::PlayOrStopScriptFunction);
  connect(lua_script_name_list_widget_ptr, &QListWidget::itemSelectionChanged, this,
          &LuaScriptWindow::UpdateButtonText);
}

void LuaScriptWindow::LoadScriptFunction()
{
  std::lock_guard<std::mutex> lock(script_start_or_stop_lock);
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getOpenFileName(
      this, tr("Select a File"),
      settings.value(QStringLiteral("luascriptwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.lua *.txt);;%2 (*)").arg(tr("All lua/txt files")).arg(tr("All Files")));

  if (!path.isEmpty())
  {
    settings.setValue(QStringLiteral("luascriptwindow/lastdir"),
                      QFileInfo(path).absoluteDir().absolutePath());
    row_num_to_is_running[next_unique_identifier] = false;
    QStringList list;
    list << path;
    lua_script_name_list_widget_ptr->addItem(path);
    lua_script_name_list_widget_ptr->setCurrentRow(lua_script_name_list_widget_ptr->count() - 1);
    ++next_unique_identifier;
    UpdateButtonText();
  }

  return;
}

void LuaScriptWindow::PlayScriptFunction()
{
  print_lock.lock();
  lua_output_list_widget_ptr->clear();
  output_lines.clear();
  print_lock.unlock();

  script_start_or_stop_lock.lock();
  int current_row = lua_script_name_list_widget_ptr->currentRow();
  std::string current_script_name =
      lua_script_name_list_widget_ptr->currentItem()->text().toStdString();
  script_start_or_stop_lock.unlock();

  Lua::Init(current_script_name, &callback_print_function,
            &finished_script_callback_function,
            current_row);

  row_num_to_is_running[current_row] = true;
}

void LuaScriptWindow::StopScriptFunction()
{
  if (!Lua::is_lua_core_initialized)
    return;

  script_start_or_stop_lock.lock();
  int current_row = lua_script_name_list_widget_ptr->currentRow();
  script_start_or_stop_lock.unlock();

  Lua::StopScript(current_row);
  row_num_to_is_running[current_row] = false;
  UpdateButtonText();
}

void LuaScriptWindow::PlayOrStopScriptFunction()
{
  if (lua_script_name_list_widget_ptr->count() == 0)
    return;

  int current_row = lua_script_name_list_widget_ptr->currentRow();
  if (row_num_to_is_running[current_row])
    StopScriptFunction();
  else if (!row_num_to_is_running[current_row])
    PlayScriptFunction();
  else
    return;
  UpdateButtonText();
}

void LuaScriptWindow::UpdateOutputWindow()
{
  std::lock_guard<std::mutex> lock(print_lock);
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
  int id_of_script_to_stop = ids_of_scripts_to_stop.pop();
  if (id_of_script_to_stop >= 0 && row_num_to_is_running[id_of_script_to_stop])
  {
    row_num_to_is_running[id_of_script_to_stop] = false;
    Lua::StopScript(id_of_script_to_stop);
    UpdateButtonText();
  }
}

void LuaScriptWindow::UpdateButtonText()
{
  if (lua_script_name_list_widget_ptr->count() == 0)
  {
    m_play_or_stop_script_button->setStyleSheet(tr("background-color:grey;"));
    m_play_or_stop_script_button->setText(tr("N/A"));
    return;
  }
  else if (row_num_to_is_running[lua_script_name_list_widget_ptr->currentRow()])
  {
    m_play_or_stop_script_button->setStyleSheet(tr("background-color:red;"));
    m_play_or_stop_script_button->setText(tr("Stop"));
  }
  else
  {
    m_play_or_stop_script_button->setStyleSheet(tr("background-color:green;"));
    m_play_or_stop_script_button->setText(tr("Play"));
  }
}
