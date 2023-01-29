// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/PostProcessingShaderWindow.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

#include "DolphinQt/Config/Graphics/GraphicsWindow.h"
#include "DolphinQt/Config/Graphics/PostProcessingShaderListWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "VideoCommon/VideoConfig.h"

constexpr const char* CUSTOM_SHADERS_DIR = "CustomShaders/";
constexpr const char* PROFILES_DIR = "Profiles/";

PostProcessingShaderWindow::PostProcessingShaderWindow(VideoCommon::PE::ShaderConfigGroup* config,
                                                       QWidget* parent)
    : QWidget(parent)
{
  setWindowTitle(tr("Postprocessing Shader Configuration"));
  setWindowIcon(Resources::GetAppIcon());

  CreateProfilesLayout();

  SetShaderGroupConfig(config);
  CreateMainLayout();
  ConnectWidgets();

  PopulateProfileSelection();

  installEventFilter(this);
}

void PostProcessingShaderWindow::SetShaderGroupConfig(VideoCommon::PE::ShaderConfigGroup* config)
{
  m_group = config;
  auto val = new PostProcessingShaderListWidget(this, config);

  if (m_pp_list_widget == nullptr)
  {
    m_pp_list_widget = val;
  }
  else
  {
    m_main_layout->replaceWidget(m_pp_list_widget, val);
    auto old = m_pp_list_widget;
    m_pp_list_widget = val;
    old->deleteLater();
  }
}

void PostProcessingShaderWindow::CreateProfilesLayout()
{
  m_profiles_layout = new QHBoxLayout();
  m_profiles_box = new QGroupBox(tr("Profile"));
  m_profiles_combo = new QComboBox();
  m_profiles_load = new QPushButton(tr("Load"));
  m_profiles_save = new QPushButton(tr("Save"));
  m_profiles_delete = new QPushButton(tr("Delete"));

  auto* button_layout = new QHBoxLayout();

  m_profiles_combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_profiles_combo->setMinimumWidth(100);
  m_profiles_combo->setEditable(true);

  m_profiles_layout->addWidget(m_profiles_combo);
  button_layout->addWidget(m_profiles_load);
  button_layout->addWidget(m_profiles_save);
  button_layout->addWidget(m_profiles_delete);
  m_profiles_layout->addLayout(button_layout);

  m_profiles_box->setLayout(m_profiles_layout);
}

void PostProcessingShaderWindow::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();
  m_config_layout = new QHBoxLayout();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  m_config_layout->addWidget(m_profiles_box);

  m_main_layout->addLayout(m_config_layout);
  m_main_layout->addWidget(m_pp_list_widget);
  m_main_layout->addWidget(m_button_box);

  setLayout(m_main_layout);
}

void PostProcessingShaderWindow::ConnectWidgets()
{
  connect(m_profiles_save, &QPushButton::clicked, this,
          &PostProcessingShaderWindow::OnSaveProfilePressed);
  connect(m_profiles_load, &QPushButton::clicked, this,
          &PostProcessingShaderWindow::OnLoadProfilePressed);
  connect(m_profiles_delete, &QPushButton::clicked, this,
          &PostProcessingShaderWindow::OnDeleteProfilePressed);

  connect(m_profiles_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &PostProcessingShaderWindow::OnSelectProfile);
  connect(m_profiles_combo, &QComboBox::editTextChanged, this,
          &PostProcessingShaderWindow::OnProfileTextChanged);

  // We currently use the "Close" button as an "Accept" button
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::hide);
}

void PostProcessingShaderWindow::UpdateProfileIndex()
{
  // Make sure currentIndex and currentData are accurate when the user manually types a name.

  const auto current_text = m_profiles_combo->currentText();
  const int text_index = m_profiles_combo->findText(current_text);
  m_profiles_combo->setCurrentIndex(text_index);

  if (text_index == -1)
    m_profiles_combo->setCurrentText(current_text);
}

void PostProcessingShaderWindow::UpdateProfileButtonState()
{
  // Make sure save/delete buttons are disabled for built-in profiles

  bool builtin = false;
  if (m_profiles_combo->findText(m_profiles_combo->currentText()) != -1)
  {
    const QString profile_path = m_profiles_combo->currentData().toString();
    builtin = profile_path.startsWith(QString::fromStdString(File::GetSysDirectory()));
  }

  m_profiles_save->setEnabled(!builtin);
  m_profiles_delete->setEnabled(!builtin);
}

void PostProcessingShaderWindow::OnSelectProfile(int)
{
  UpdateProfileButtonState();
}

void PostProcessingShaderWindow::OnProfileTextChanged(const QString&)
{
  UpdateProfileButtonState();
}

void PostProcessingShaderWindow::OnDeleteProfilePressed()
{
  UpdateProfileIndex();

  const QString profile_name = m_profiles_combo->currentText();
  const QString profile_path = m_profiles_combo->currentData().toString();

  if (m_profiles_combo->currentIndex() == -1 || !File::Exists(profile_path.toStdString()))
  {
    ModalMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("The profile '%1' does not exist").arg(profile_name));
    error.exec();
    return;
  }

  ModalMessageBox confirm(this);

  confirm.setIcon(QMessageBox::Warning);
  confirm.setWindowTitle(tr("Confirm"));
  confirm.setText(tr("Are you sure that you want to delete '%1'?").arg(profile_name));
  confirm.setInformativeText(tr("This cannot be undone!"));
  confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if (confirm.exec() != QMessageBox::Yes)
  {
    return;
  }

  m_profiles_combo->removeItem(m_profiles_combo->currentIndex());
  m_profiles_combo->setCurrentIndex(-1);

  File::Delete(profile_path.toStdString());

  ModalMessageBox result(this);
  result.setIcon(QMessageBox::Information);
  result.setWindowModality(Qt::WindowModal);
  result.setWindowTitle(tr("Success"));
  result.setText(tr("Successfully deleted '%1'.").arg(profile_name));
}

void PostProcessingShaderWindow::OnLoadProfilePressed()
{
  UpdateProfileIndex();

  if (m_profiles_combo->currentIndex() == -1)
  {
    ModalMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("The profile '%1' does not exist").arg(m_profiles_combo->currentText()));
    error.exec();
    return;
  }

  const QString profile_path = m_profiles_combo->currentData().toString();
  const auto profile_path_str = profile_path.toStdString();

  std::string json_data;
  if (!File::ReadFileToString(profile_path_str, json_data))
  {
    ModalMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("Failed to open the profile path '%1'").arg(profile_path));
    error.exec();
    return;
  }

  picojson::value root;
  const auto error = picojson::parse(root, json_data);

  if (!error.empty())
  {
    ModalMessageBox message_box(this);
    message_box.setIcon(QMessageBox::Critical);
    message_box.setWindowTitle(tr("Error"));
    message_box.setText(tr("Failed to parse the profile path '%1' due to the error '%2'")
                            .arg(profile_path, QString::fromStdString(error)));
    message_box.exec();
    return;
  }

  if (!root.is<picojson::array>())
  {
    ModalMessageBox message_box(this);
    message_box.setIcon(QMessageBox::Critical);
    message_box.setWindowTitle(tr("Error"));
    message_box.setText(
        tr("Failed to load the profile path '%1', root must contain an array!").arg(profile_path));
    return;
  }

  const auto serialized_shaders = root.get<picojson::array>();
  m_group->DeserializeFromProfile(serialized_shaders);
  SetShaderGroupConfig(m_group);
}

void PostProcessingShaderWindow::OnSaveProfilePressed()
{
  const QString profile_name = m_profiles_combo->currentText();

  if (profile_name.isEmpty())
    return;

  const std::string profile_path = File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR +
                                   CUSTOM_SHADERS_DIR + profile_name.toStdString() + ".ini";

  File::CreateFullPath(profile_path);

  if (m_profiles_combo->findText(profile_name) == -1)
  {
    PopulateProfileSelection();
    m_profiles_combo->setCurrentIndex(m_profiles_combo->findText(profile_name));
  }

  std::ofstream json_stream;
  File::OpenFStream(json_stream, profile_path, std::ios_base::out);
  if (!json_stream.is_open())
  {
    ModalMessageBox message_box(this);
    message_box.setIcon(QMessageBox::Critical);
    message_box.setWindowTitle(tr("Error"));
    message_box.setText(tr("Failed to open the profile path '%1' for writing")
                            .arg(QString::fromStdString(profile_path)));
    message_box.exec();
    return;
  }

  picojson::value serialized_shaders;
  m_group->SerializeToProfile(serialized_shaders);

  const auto output = serialized_shaders.serialize(true);
  json_stream << output;
}

void PostProcessingShaderWindow::PopulateProfileSelection()
{
  m_profiles_combo->clear();

  const std::string profiles_path =
      File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR + CUSTOM_SHADERS_DIR;
  for (const auto& filename : Common::DoFileSearch({profiles_path}, {".ini"}))
  {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    m_profiles_combo->addItem(QString::fromStdString(basename), QString::fromStdString(filename));
  }

  m_profiles_combo->setCurrentIndex(-1);
}

bool PostProcessingShaderWindow::eventFilter(QObject* object, QEvent* event)
{
  // Close when escape is pressed
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Cancel))
      hide();
  }

  return false;
}
