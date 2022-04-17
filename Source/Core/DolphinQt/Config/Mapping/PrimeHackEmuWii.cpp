// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/PrimeHackEmuWii.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"

#include "Core/PrimeHack/HackConfig.h"

class PrimeHackModes;
constexpr const char* PROFILES_DIR = "Profiles/";

PrimeHackEmuWii::PrimeHackEmuWii(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
  Connect(window);
  LoadSettings();
}

void PrimeHackEmuWii::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  auto* groupbox1 = new QVBoxLayout();
  auto* groupbox2 = new QVBoxLayout();

  auto* modes_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes);
  auto* modes = CreateGroupBox(tr("Mode"), modes_group);

  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(modes_group);

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->setAlignment(Qt::AlignCenter);
  combo_hbox->setSpacing(10);

  combo_hbox->addWidget(new QLabel(tr("Mouse")));
  combo_hbox->addWidget(m_radio_mouse = new QRadioButton());
  combo_hbox->addSpacing(95);
  combo_hbox->addWidget(new QLabel(tr("Controller")));
  combo_hbox->addWidget(m_radio_controller = new QRadioButton());

  modes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  static_cast<QFormLayout*>(modes->layout())->insertRow(0, combo_hbox);

  groupbox1->addWidget(modes, 0);

  auto* misc_box = CreateGroupBox(tr("Miscellaneous"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));

  misc_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox1->addWidget(misc_box, 0);

  //TODO:  Look into shrinking this by MAYBE shrinking the button padding to 1 instead of 2???
  m_morphball_combobox = new QComboBox();
  m_morphball_combobox->setObjectName(tr("ProfileList"));
  m_morphball_combobox->setToolTip(tr("Set the controller profile to use\nwhen in Morph Ball and Map."));
  m_morphball_combobox->setEditable(false);
  QFormLayout* misc_box_layout = static_cast<QFormLayout*>(misc_box->layout());
  misc_box_layout->addRow(tr("Morphball & Map Profile"), m_morphball_combobox);

  PopulateMorphBallProfiles();

  auto* beam_box = CreateGroupBox(tr("Beams"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Beams));

  beam_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  groupbox1->addWidget(beam_box, 1, Qt::AlignTop);

  auto* visor_box = 
    CreateGroupBox(tr("Visors"), Wiimote::GetWiimoteGroup(
      GetPort(), WiimoteEmu::WiimoteGroup::Visors));

  visor_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  groupbox1->addWidget(visor_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox1, 0, 0);

  auto* camera = 
    CreateGroupBox(tr("Camera"),
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Camera));

  groupbox2->addWidget(camera, 0, Qt::AlignTop);

  controller_box = CreateGroupBox(tr("Camera Control"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::ControlStick));

  controller_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  controller_box->setEnabled(ce_modes->GetSelectedDevice() == 1);

  groupbox2->addWidget(controller_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox2, 0, 1, Qt::AlignTop);

  setLayout(layout);
}

void PrimeHackEmuWii::OnMorphControlSelectionChanged()
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

void PrimeHackEmuWii::UpdateMorphProfileBackupFile()
{
  const std::string og_wiimote_new = WIIMOTE_INI_NAME;
  const std::string backup_wiimote_new = std::string(WIIMOTE_INI_NAME) + "_Backup.ini";

  const std::string default_path = File::GetUserPath(D_CONFIG_IDX) + og_wiimote_new + ".ini";
  std::string file_text;
  if (File::ReadFileToString(default_path, file_text))
    File::WriteStringToFile(File::GetUserPath(D_CONFIG_IDX) + backup_wiimote_new, file_text);
}

void PrimeHackEmuWii::PopulateMorphBallProfiles()
{
  m_morphball_combobox->clear();

  const std::string profiles_path =
    File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR + GetConfig()->GetProfileName();

  //Add default value
  m_morphball_combobox->addItem(QString::fromStdString(std::string("Disabled")),
    QString::fromStdString(std::string("Disabled")));

  for (const auto& filename : Common::DoFileSearch({ profiles_path }, { ".ini" }))
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
  for (const auto& filename : Common::DoFileSearch({ builtin_profiles_path }, { ".ini" }))
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

void PrimeHackEmuWii::MappingWindowProfileSave()
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

void PrimeHackEmuWii::MappingWindowProfileLoad()
{
  PopulateMorphBallProfiles();
  SaveSettings();

  // Backup WiimoteNew.ini to make sure we have the user's control scheme.
  UpdateMorphProfileBackupFile();
}

void PrimeHackEmuWii::Connect(MappingWindow* window)
{
  connect(window, &MappingWindow::ProfileSaved, this, &PrimeHackEmuWii::MappingWindowProfileSave);
  connect(window, &MappingWindow::ProfileLoaded, this, &PrimeHackEmuWii::MappingWindowProfileLoad);
  connect(window, &MappingWindow::finished, this, &PrimeHackEmuWii::OnMorphControlSelectionChanged);
  connect(window, &MappingWindow::rejected, this, &PrimeHackEmuWii::MappingWindowProfileSave);
  connect(m_morphball_combobox, &QComboBox::textActivated, this, &PrimeHackEmuWii::MappingWindowProfileSave);

  connect(window, &MappingWindow::ConfigChanged, this, &PrimeHackEmuWii::ConfigChanged);
  connect(window, &MappingWindow::Update, this, &PrimeHackEmuWii::Update);
  connect(m_radio_mouse, &QRadioButton::toggled, this,
    &PrimeHackEmuWii::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &PrimeHackEmuWii::OnDeviceSelected);
}

void PrimeHackEmuWii::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_mouse->isChecked() ? 0 : 1);
  controller_box->setEnabled(!m_radio_mouse->isChecked());

  ConfigChanged();
  SaveSettings();
}

void PrimeHackEmuWii::ConfigChanged()
{
  Update();

  prime::UpdateHackSettings();
}

void PrimeHackEmuWii::Update()
{
  bool checked = Wiimote::PrimeUseController();

  controller_box->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_mouse->setChecked(!checked);
  }
}

void PrimeHackEmuWii::LoadSettings()
{
  Wiimote::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.

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
  controller_box->setEnabled(!checked);
}

void PrimeHackEmuWii::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* PrimeHackEmuWii::GetConfig()
{
  return Wiimote::GetConfig();
}
