#include "DolphinQt/ScriptWindow.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include "Core/CoreTiming.h"
#include "Core/System.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

static void (*callback_print_function)(void*, const char*);
static void (*finished_script_callback_function)(void*, int);
static CoreTiming::EventType* stop_script_from_callback_event = nullptr;
static ScriptWindow* copy_of_window = nullptr;
static ScriptWindow* GetThis()
{
  return copy_of_window;
}

ScriptWindow::ScriptWindow(QWidget* parent) : QDialog(parent)
{
  copy_of_window = this;
  stop_script_from_callback_event = Core::System::GetInstance().GetCoreTiming().RegisterEvent(
      "SCRIPT_STOP_FUNC", [](Core::System& system_, u64 script_id, s64) {
        Scripting::ScriptUtilities::PushScriptStopQueueEvent(
            ScriptQueueEventTypes::StopScriptFromScriptEndCallback, script_id);
      });

  next_unique_identifier = 1;
  callback_print_function = [](void* x, const char* message) {
    std::lock_guard<std::mutex> lock(GetThis()->print_lock);
    GetThis()->output_lines.push_back(message);
    QueueOnObject(GetThis(), &ScriptWindow::UpdateOutputWindow);
  };

  finished_script_callback_function = [](void* base_script_context_ptr, int unique_identifier) {
    std::lock_guard<std::mutex> lock(GetThis()->script_start_or_stop_lock);
    GetThis()->ids_of_scripts_to_stop.push(unique_identifier);
    Scripting::ScriptUtilities::SetIsScriptActiveToFalse(base_script_context_ptr);
    Core::System::GetInstance().GetCoreTiming().ScheduleEvent(
        -1, stop_script_from_callback_event, unique_identifier, CoreTiming::FromThread::ANY);
    QueueOnObject(GetThis(), &ScriptWindow::OnScriptFinish);
  };

  CreateMainLayout();
  ConnectWidgets();
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("scriptwindow/geometry")).toByteArray());
}

ScriptWindow::~ScriptWindow()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("scriptwindow/geometry"), saveGeometry());
}

void ScriptWindow::CreateMainLayout()
{
  auto* layout = new QGridLayout();
  m_load_script_button = new NonDefaultQPushButton(tr("Load Script"));

  m_play_or_stop_script_button = new NonDefaultQPushButton(tr("N/A"));
  m_play_or_stop_script_button->setStyleSheet(tr("background-color:grey;"));

  script_name_list_widget_ptr = new QListWidget;
  script_name_list_widget_ptr->setMinimumHeight(100);
  script_name_list_widget_ptr->setMaximumHeight(200);
  script_name_list_widget_ptr->setMinimumWidth(550);
  script_name_list_widget_ptr->setMaximumWidth(800);

  QLabel* script_name_header = new QLabel(tr("Script Name:"));
  auto* script_name_box = new QGridLayout();
  script_name_box->addWidget(script_name_header, 0, 0, Qt::AlignTop);
  script_name_box->addWidget(script_name_list_widget_ptr, 1, 0, Qt::AlignTop);

  QLabel* output_header = new QLabel(tr("Output:"));
  script_output_list_widget_ptr = new QListWidget;
  script_output_list_widget_ptr->setMinimumHeight(300);
  script_output_list_widget_ptr->setMaximumHeight(600);
  script_output_list_widget_ptr->setMinimumWidth(550);
  script_output_list_widget_ptr->setMaximumWidth(800);
  auto* output_box = new QGridLayout();
  output_box->addWidget(output_header, 0, 0, Qt::AlignTop);
  output_box->addWidget(script_output_list_widget_ptr, 1, 0, Qt::AlignTop);

  script_output_list_widget_ptr->setSpacing(1);
  script_output_list_widget_ptr->setStyleSheet(tr("background-color:white;"));

  layout->addWidget(m_load_script_button, 0, 0, Qt::AlignTop);
  layout->addWidget(m_play_or_stop_script_button, 0, 1, Qt::AlignTop);
  layout->addLayout(script_name_box, 1, 0, Qt::AlignTop);
  layout->addLayout(output_box, 1, 1, Qt::AlignTop);
  setWindowTitle(tr("Script Manager"));
  setLayout(layout);
}

void ScriptWindow::ConnectWidgets()
{
  connect(m_load_script_button, &QPushButton::clicked, this, &ScriptWindow::LoadScriptFunction);
  connect(m_play_or_stop_script_button, &QPushButton::clicked, this,
          &ScriptWindow::PlayOrStopScriptFunction);
  connect(script_name_list_widget_ptr, &QListWidget::itemSelectionChanged, this,
          &ScriptWindow::UpdateButtonText);
}

void ScriptWindow::LoadScriptFunction()
{
  std::lock_guard<std::mutex> lock(script_start_or_stop_lock);
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getOpenFileName(
      this, tr("Select a File"),
      settings.value(QStringLiteral("scriptwindow/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.lua *.txt *.py);;%3 (*)")
          .arg(tr("All Lua/txt files"))
          .arg(tr("All Python files"))
          .arg(tr("All Files")));

  if (!path.isEmpty())
  {
    settings.setValue(QStringLiteral("scriptwindow/lastdir"),
                      QFileInfo(path).absoluteDir().absolutePath());
    row_num_to_is_running[next_unique_identifier] = false;
    QStringList list;
    list << path;
    script_name_list_widget_ptr->addItem(path);
    script_name_list_widget_ptr->setCurrentRow(script_name_list_widget_ptr->count() - 1);
    ++next_unique_identifier;
    UpdateButtonText();
  }

  return;
}

void ScriptWindow::PlayScriptFunction()
{
  print_lock.lock();
  script_output_list_widget_ptr->clear();
  output_lines.clear();
  print_lock.unlock();

  script_start_or_stop_lock.lock();
  int current_row = script_name_list_widget_ptr->currentRow() + 1;
  std::string current_script_name =
      script_name_list_widget_ptr->currentItem()->text().toStdString();
  script_start_or_stop_lock.unlock();

  row_num_to_is_running[current_row] = true;

  Scripting::ScriptUtilities::PushScriptCreateQueueEvent(current_row, current_script_name.c_str(),
                                                         callback_print_function,
                                                         finished_script_callback_function);
}

void ScriptWindow::StopScriptFunction()
{
  if (!Scripting::ScriptUtilities::IsScriptingCoreInitialized())
    return;

  script_start_or_stop_lock.lock();
  int current_row = script_name_list_widget_ptr->currentRow() + 1;
  script_start_or_stop_lock.unlock();
  Scripting::ScriptUtilities::PushScriptStopQueueEvent(ScriptQueueEventTypes::StopScriptFromUI,
                                                       current_row);
  row_num_to_is_running[current_row] = false;
  UpdateButtonText();
}

void ScriptWindow::PlayOrStopScriptFunction()
{
  if (script_name_list_widget_ptr->count() == 0)
    return;

  int current_row = script_name_list_widget_ptr->currentRow() + 1;
  if (row_num_to_is_running[current_row])
    StopScriptFunction();
  else if (!row_num_to_is_running[current_row])
    PlayScriptFunction();
  else
    return;
  UpdateButtonText();
}

void ScriptWindow::UpdateOutputWindow()
{
  std::lock_guard<std::mutex> lock(print_lock);
  QStringList list;
  size_t output_size = output_lines.size();
  for (size_t i = 0; i < output_size; ++i)
    list << QString::fromStdString(output_lines[i]);
  script_output_list_widget_ptr->clear();
  script_output_list_widget_ptr->insertItems(0, list);
  script_output_list_widget_ptr->setSpacing(1);
}

void ScriptWindow::OnScriptFinish()
{
  int id_of_script_to_stop = ids_of_scripts_to_stop.pop();
  if (id_of_script_to_stop > 0 && row_num_to_is_running[id_of_script_to_stop])
  {
    row_num_to_is_running[id_of_script_to_stop] = false;
    UpdateButtonText();
  }
}

void ScriptWindow::UpdateButtonText()
{
  if (script_name_list_widget_ptr->count() == 0)
  {
    m_play_or_stop_script_button->setStyleSheet(tr("background-color:grey;"));
    m_play_or_stop_script_button->setText(tr("N/A"));
    return;
  }
  else if (row_num_to_is_running[script_name_list_widget_ptr->currentRow() + 1])
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
