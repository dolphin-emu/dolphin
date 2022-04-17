// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/WiimoteEmuMetroid.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>

#include "Common/FileUtil.h"
#include "Common/FileSearch.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/PrimeHack/HackConfig.h"
#include "Core/PrimeHack/HackManager.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"
#include "InputCommon/InputConfig.h"


#include <QDesktopServices>
#include <QUrl>

constexpr const char* PROFILES_DIR = "Profiles/";

WiimoteEmuMetroid::WiimoteEmuMetroid(MappingWindow* window, WiimoteEmuExtension* extension)
    : MappingWidget(window), m_extension_widget(extension)
{
  CreateMainLayout();
  Connect();

  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  // Nunchuk
  ce_extension->SetSelectedAttachment(1);

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuMetroid::UpdateMorphProfileBackupFile()
{
  const std::string og_wiimote_new = WIIMOTE_INI_NAME;
  const std::string backup_wiimote_new = std::string(WIIMOTE_INI_NAME) + "_Backup.ini";

  const std::string default_path = File::GetUserPath(D_CONFIG_IDX) + og_wiimote_new + ".ini";
  std::string file_text;
  if (File::ReadFileToString(default_path, file_text))
    File::WriteStringToFile(File::GetUserPath(D_CONFIG_IDX) + backup_wiimote_new, file_text);
}

void WiimoteEmuMetroid::PopulateMorphBallProfiles()
{
  m_morphball_combobox->clear();

  const std::string profiles_path =
      File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR + GetConfig()->GetProfileName();

  //Add default value
  m_morphball_combobox->addItem(QString::fromStdString(std::string("Disabled")),
                                      QString::fromStdString(std::string("Disabled")));

  for (const auto& filename : Common::DoFileSearch({profiles_path}, {".ini"}))
  {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    if (!basename.empty())  // Ignore files with an empty name to avoid multiple problems
      m_morphball_combobox->addItem(QString::fromStdString(basename),
                                      QString::fromStdString(filename));
  }

  m_morphball_combobox->insertSeparator(m_morphball_combobox->count());

  const std::string builtin_profiles_path =
      File::GetSysDirectory() + PROFILES_DIR + GetConfig()->GetProfileName();
  for (const auto& filename : Common::DoFileSearch({builtin_profiles_path}, {".ini"}))
  {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    if (!basename.empty())
    {
      // i18n: "Stock" refers to input profiles included with Dolphin
      m_morphball_combobox->addItem(tr("%1 (Stock)").arg(QString::fromStdString(basename)),
                                      QString::fromStdString(filename));
    }
  }

  auto* morph_group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::AltProfileControls));

  QString text = tr(morph_group->GetAltProfileName().c_str());
  std::string cstring = morph_group->GetAltProfileName();
  m_morphball_combobox->setCurrentIndex(m_morphball_combobox->findText(text));
}

void WiimoteEmuMetroid::CreateMainLayout()
{
  auto* layout = new QHBoxLayout;

  // Column 0

  auto* groupbox0 = new QVBoxLayout();

  auto* wiimote_buttons = CreateGroupBox(tr("Wiimote Buttons"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Buttons));

  wiimote_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(wiimote_buttons);

  auto* nunchuk_buttons = CreateGroupBox(tr("Nunchuk Buttons"),
    Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons));

  nunchuk_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(nunchuk_buttons);

  auto* wiimote_options = CreateGroupBox(tr("Wiimote Properties"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Options));
  groupbox0->addWidget(wiimote_options, 1);

  QGroupBox* help_box = new QGroupBox(tr("Help"));
  const auto help_hbox = new QHBoxLayout;

  m_help_button = new QPushButton();
  m_help_button->setText(tr("Open Wiki Page"));
  connect(m_help_button, &QPushButton::clicked, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/shiiion/dolphin/wiki/Installation#changing-primehack-settings")));
  });

  help_hbox->addWidget(m_help_button);
  help_box->setLayout(help_hbox);

  help_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(help_box);

  // Column 1

  auto* groupbox1 = new QVBoxLayout();

  auto* beam_box = CreateGroupBox(tr("Beams"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Beams));
  groupbox1->addWidget(beam_box);

  auto* visor_box = CreateGroupBox(tr("Visors"), Wiimote::GetWiimoteGroup(
      GetPort(), WiimoteEmu::WiimoteGroup::Visors));
  groupbox1->addWidget(visor_box);

  auto* morphball_control_box = CreateGroupBox(tr("Morphball & Map Profile"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::AltProfileControls));
  groupbox1->addWidget(morphball_control_box);
  m_morphball_combobox = (morphball_control_box->findChild<QComboBox*>(tr("ProfileList")));
  m_morphball_combobox->setToolTip(tr("Set the controller profile to use\nwhen in Morph Ball and Map."));
  m_morphball_combobox->setEditable(false);

  PopulateMorphBallProfiles();


  auto* rumble_box = CreateGroupBox(tr("Rumble"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::Rumble));
  groupbox1->addWidget(rumble_box, 1);

  // Column 2

  auto* groupbox2 = new QVBoxLayout();

  auto* misc_box = CreateGroupBox(tr("Motion Interactions"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));
  groupbox2->addWidget(misc_box, 0, Qt::AlignTop);

  auto* movement_stick = CreateGroupBox(tr("Movement Stick"), Wiimote::GetNunchukGroup(
    GetPort(), WiimoteEmu::NunchukGroup::Stick));

  movement_stick->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  groupbox2->addWidget(movement_stick, 1);

  // Column 3

  auto* groupbox3 = new QVBoxLayout();

  auto* modes_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes);
  auto* modes = CreateGroupBox(tr("Mode"), modes_group);

  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(modes_group);

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->setAlignment(Qt::AlignCenter);
  combo_hbox->setSpacing(10);

  combo_hbox->addWidget(new QLabel(tr("Mouse")));
  combo_hbox->addWidget(m_radio_mouse = new QRadioButton());
  combo_hbox->addSpacing(65);
  combo_hbox->addWidget(new QLabel(tr("Controller")));
  combo_hbox->addWidget(m_radio_controller = new QRadioButton());

  modes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  static_cast<QFormLayout*>(modes->layout())->insertRow(0, combo_hbox);
  groupbox3->addWidget(modes, 0);

  auto* camera_options = 
    CreateGroupBox(tr("Camera"),
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Camera));
  groupbox3->addWidget(camera_options, 0, Qt::AlignTop);

  camera_control = CreateGroupBox(tr("Camera Control"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::ControlStick));
  camera_control->setEnabled(ce_modes->GetSelectedDevice() == 1);
  groupbox3->addWidget(camera_control, 1);

  layout->addLayout(groupbox0);
  layout->addLayout(groupbox1);
  layout->addLayout(groupbox2);
  layout->addLayout(groupbox3);

  setLayout(layout);
}

void WiimoteEmuMetroid::Connect()
{
  MappingWindow* parent = GetParent();
  connect(parent, &MappingWindow::ProfileSaved, this, &WiimoteEmuMetroid::MappingWindowProfileSave);
  connect(parent, &MappingWindow::ProfileLoaded, this, &WiimoteEmuMetroid::MappingWindowProfileLoad);
  connect(this, &MappingWidget::ConfigChanged, this, &WiimoteEmuMetroid::ConfigChanged);
  connect(this, &MappingWidget::Update, this, &WiimoteEmuMetroid::Update);
  connect(m_radio_mouse, &QRadioButton::toggled, this,
    &WiimoteEmuMetroid::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &WiimoteEmuMetroid::OnDeviceSelected);

  connect(parent, &MappingWindow::finished, this, &WiimoteEmuMetroid::OnMorphControlSelectionChanged);
  connect(parent, &MappingWindow::rejected, this, &WiimoteEmuMetroid::MappingWindowProfileSave);
  connect(m_morphball_combobox, &QComboBox::textActivated, this, &WiimoteEmuMetroid::MappingWindowProfileSave);
}

void WiimoteEmuMetroid::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_mouse->isChecked() ? 0 : 1);
  camera_control->setEnabled(!m_radio_mouse->isChecked());

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuMetroid::OnMorphControlSelectionChanged()
{
  //Called as soon as our selection is changed to update the controller preset for Morphball mode.
  auto* morph_group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::AltProfileControls));

  std::string curr_text = m_morphball_combobox->currentText().toStdString();
  if (!curr_text.empty())
    morph_group->SetAltProfileName(curr_text);

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuMetroid::MappingWindowProfileLoad()
{
  PopulateMorphBallProfiles();
  SaveSettings();

  // Backup WiimoteNew.ini to make sure we have the user's control scheme.
  UpdateMorphProfileBackupFile();
}

void WiimoteEmuMetroid::MappingWindowProfileSave()
{
  auto* morph_group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::AltProfileControls));

  std::string curr_text = m_morphball_combobox->currentText().toStdString();
  if (!curr_text.empty())
    morph_group->SetAltProfileName(curr_text);

  PopulateMorphBallProfiles();
  SaveSettings();

  // Backup WiimoteNew.ini to make sure we have the user's control scheme.
  UpdateMorphProfileBackupFile();
}

void WiimoteEmuMetroid::ConfigChanged()
{
  return; // All this does is update the extension UI, which we do not have.
}

void WiimoteEmuMetroid::Update()
{
  bool checked = Wiimote::PrimeUseController();

  camera_control->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_mouse->setChecked(!checked);
  }
}

void WiimoteEmuMetroid::LoadSettings()
{
  Wiimote::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.

  auto* morph_group = static_cast<ControllerEmu::PrimeHackAltProfile*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::AltProfileControls));

  auto* modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  bool checked;

  // Do not allow mouse mode on platforms with input APIs we do not support.
  if (modes->GetMouseSupported()) {
    checked = modes->GetSelectedDevice() == 0;
  }
  else {
    checked = 1;
    m_radio_mouse->setEnabled(false);
    m_radio_controller->setEnabled(false);
  }

  m_radio_mouse->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  camera_control->setEnabled(!checked);

  QString text = tr(morph_group->GetAltProfileName().c_str());

  m_morphball_combobox->setCurrentIndex(m_morphball_combobox->findText(text));

}

void WiimoteEmuMetroid::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* WiimoteEmuMetroid::GetConfig()
{
  return Wiimote::GetConfig();
}
