#include "DolphinQt/ScriptManager.h"

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
static ScriptManager* copy_of_window = nullptr;

ScriptManager::ScriptManager(QWidget* parent) : QDialog(parent)
{
  copy_of_window = this;
  stop_script_from_callback_event = Core::System::GetInstance().GetCoreTiming().RegisterEvent(
      "SCRIPT_STOP_FUNC", [](Core::System& system_, u64 script_id, s64) {
        // TODO: Implement this function.
      });

  next_unique_identifier = 1;
  callback_print_function = [](void* x, const char* message) {
    // TODO: Implement this function.
  };

  finished_script_callback_function = [](void* base_script_context_ptr, int unique_identifier) {
    // TODO: Implement this function.
  };

  CreateMainLayout();
  ConnectWidgets();
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("scriptmanager/geometry")).toByteArray());
}

ScriptManager::~ScriptManager()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("scriptmanager/geometry"), saveGeometry());
}

void ScriptManager::CreateMainLayout()
{
  auto* layout = new QGridLayout();
  m_load_script_button = new NonDefaultQPushButton(QStringLiteral("Load Script"));

  m_play_or_stop_script_button = new NonDefaultQPushButton(QStringLiteral("N/A"));
  m_play_or_stop_script_button->setStyleSheet(QStringLiteral("background-color:grey;"));

  script_name_list_widget_ptr = new QListWidget;
  script_name_list_widget_ptr->setMinimumHeight(100);
  script_name_list_widget_ptr->setMaximumHeight(200);
  script_name_list_widget_ptr->setMinimumWidth(550);
  script_name_list_widget_ptr->setMaximumWidth(800);

  QLabel* script_name_header = new QLabel(QStringLiteral("Script Name:"));
  auto* script_name_box = new QGridLayout();
  script_name_box->addWidget(script_name_header, 0, 0, Qt::AlignTop);
  script_name_box->addWidget(script_name_list_widget_ptr, 1, 0, Qt::AlignTop);

  QLabel* output_header = new QLabel(QStringLiteral("Output:"));
  script_output_list_widget_ptr = new QListWidget;
  script_output_list_widget_ptr->setMinimumHeight(300);
  script_output_list_widget_ptr->setMaximumHeight(600);
  script_output_list_widget_ptr->setMinimumWidth(550);
  script_output_list_widget_ptr->setMaximumWidth(800);
  auto* output_box = new QGridLayout();
  output_box->addWidget(output_header, 0, 0, Qt::AlignTop);
  output_box->addWidget(script_output_list_widget_ptr, 1, 0, Qt::AlignTop);

  script_output_list_widget_ptr->setSpacing(1);
  script_output_list_widget_ptr->setStyleSheet(QStringLiteral("background-color:white;"));

  layout->addWidget(m_load_script_button, 0, 0, Qt::AlignTop);
  layout->addWidget(m_play_or_stop_script_button, 0, 1, Qt::AlignTop);
  layout->addLayout(script_name_box, 1, 0, Qt::AlignTop);
  layout->addLayout(output_box, 1, 1, Qt::AlignTop);
  setWindowTitle(QStringLiteral("Script Manager"));
  setLayout(layout);
}

void ScriptManager::ConnectWidgets()
{
  connect(m_load_script_button, &QPushButton::clicked, this, &ScriptManager::LoadScriptFunction);
  connect(m_play_or_stop_script_button, &QPushButton::clicked, this,
          &ScriptManager::PlayOrStopScriptFunction);
  connect(script_name_list_widget_ptr, &QListWidget::itemSelectionChanged, this,
          &ScriptManager::UpdateButtonText);
}

void ScriptManager::LoadScriptFunction()
{
  std::lock_guard<std::mutex> lock(script_start_or_stop_lock);
  auto& settings = Settings::Instance().GetQSettings();
  QString path = DolphinFileDialog::getOpenFileName(
      this, QStringLiteral("Select a File"),
      settings.value(QStringLiteral("scriptmanager/lastdir"), QString{}).toString(),
      QStringLiteral("%1 (*.lua *.txt *.py);;%3 (*)")
          .arg(QStringLiteral("All Lua/txt files"))
          .arg(QStringLiteral("All Python files"))
          .arg(QStringLiteral("All Files")));

  if (!path.isEmpty())
  {
    settings.setValue(QStringLiteral("scriptmanager/lastdir"),
                      QFileInfo(path).absoluteDir().absolutePath());
    row_num_to_is_running[next_unique_identifier] = false;
    script_name_list_widget_ptr->addItem(path);
    script_name_list_widget_ptr->setCurrentRow(script_name_list_widget_ptr->count() - 1);
    ++next_unique_identifier;
    UpdateButtonText();
  }
}

void ScriptManager::PlayScriptFunction()
{
  // TODO: Implement this function.
}

void ScriptManager::StopScriptFunction()
{
  // TODO: Implement this function.
}

void ScriptManager::PlayOrStopScriptFunction()
{
  // TODO: Implement this function.
}

void ScriptManager::UpdateOutputWindow()
{
  // TODO: Implement this function.
}

void ScriptManager::OnScriptFinish()
{
  // TODO: Implement this function.
}

void ScriptManager::UpdateButtonText()
{
  // TODO: Implement this function.
}
