#include "LuaScriptWindow.h"
#include <QDialogButtonBox>
#include <QListWidget>
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

LuaScriptWindow::LuaScriptWindow(QWidget* parent) : QDialog(parent)
{
  callbackPrintFunction = [this](const std::string& message) {
    outputLines.push_back(message);
    QueueOnObject(this, &LuaScriptWindow::updateOutputWindow);
  };

  finishedScriptCallbackFunction = [this]() {
    QueueOnObject(this, &LuaScriptWindow::onScriptFinish);
  };
  CreateMainLayout();
  ConnectWidgets();
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("luascriptwindow/geometry")).toByteArray());
}

LuaScriptWindow::~LuaScriptWindow()
{
  Lua::luaScriptActive = false;
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

  QLabel* scriptNameHeader = new QLabel(tr("Script Name:"));
  auto* scriptNameBox = new QGridLayout();
  scriptNameBox->addWidget(scriptNameHeader, 0, 0, Qt::AlignTop);
  scriptNameBox->addWidget(lua_script_name_list_widget_ptr, 1, 0, Qt::AlignTop);

  QLabel* outputHeader = new QLabel(tr("Output:"));
  lua_output_list_widget_ptr = new QListWidget;
  auto* outputBox = new QGridLayout();
  outputBox->addWidget(outputHeader, 0, 0, Qt::AlignTop);
  outputBox->addWidget(lua_output_list_widget_ptr, 1, 0, Qt::AlignTop);

   QStringList list;
  list << QString::fromStdString("Hello World!");
  list << QString::fromStdString("Last line...\n\nOther line!");
  lua_output_list_widget_ptr->insertItems(0, list);
  lua_output_list_widget_ptr->setSpacing(1);
  lua_output_list_widget_ptr->setStyleSheet(tr("background-color:white;"));

  layout->addWidget(m_load_script_button, 0, 0, Qt::AlignTop);
  layout->addWidget(m_play_script_button, 0, 1, Qt::AlignTop);
  layout->addWidget(m_stop_script_button, 0, 2, Qt::AlignTop);
  layout->addLayout(scriptNameBox, 1, 0, Qt::AlignTop);
  layout->addLayout(outputBox, 1, 1, Qt::AlignTop);
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
    pathOfScriptToRun = path.toStdString();
    QStringList list;
    list << path;
    lua_script_name_list_widget_ptr->insertItems(0, list);
    m_play_script_button->setStyleSheet(tr("background-color:green;"));
  }


  return;
}

void LuaScriptWindow::PlayScriptFunction()
{
  if (Lua::luaScriptActive || pathOfScriptToRun.empty())
    return;

  lua_output_list_widget_ptr->clear();
  Lua::Init(pathOfScriptToRun, &callbackPrintFunction, &finishedScriptCallbackFunction);
  m_play_script_button->setStyleSheet(tr("background-color:grey;"));
  m_stop_script_button->setStyleSheet(tr("background-color:red;"));
}

void LuaScriptWindow::StopScriptFunction()
{
  if (!Lua::luaInitialized || !Lua::luaScriptActive)
    return;

  Lua::StopScript();
  m_play_script_button->setStyleSheet(tr("background-color:green;"));
  m_stop_script_button->setStyleSheet(tr("background-color:grey;"));
}

void LuaScriptWindow::updateOutputWindow()
{
  QStringList list;
  size_t outputSize = outputLines.size();
  for (size_t i = 0; i < outputSize; ++i)
    list << QString::fromStdString(outputLines[i]);
  lua_output_list_widget_ptr->insertItems(0, list);
  lua_output_list_widget_ptr->setSpacing(1);
}

void LuaScriptWindow::onScriptFinish()
{
  m_play_script_button->setStyleSheet(tr("background-color:green;"));
  m_stop_script_button->setStyleSheet(tr("background-color:grey;"));
}
