// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/GameCubePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/GCMemcardManager.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings.h"

enum
{
  SLOT_A_INDEX,
  SLOT_B_INDEX,
  SLOT_SP1_INDEX,
  SLOT_COUNT
};

GameCubePane::GameCubePane()
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
}

void GameCubePane::CreateWidgets()
{
  QVBoxLayout* layout = new QVBoxLayout(this);

  // IPL Settings
  QGroupBox* ipl_box = new QGroupBox(tr("IPL Settings"), this);
  QGridLayout* ipl_layout = new QGridLayout(ipl_box);
  ipl_box->setLayout(ipl_layout);

  m_skip_main_menu = new QCheckBox(tr("Skip Main Menu"), ipl_box);
  m_language_combo = new QComboBox(ipl_box);
  m_language_combo->setCurrentIndex(-1);

  // Add languages
  for (const auto& entry : {std::make_pair(tr("English"), 0), std::make_pair(tr("German"), 1),
                            std::make_pair(tr("French"), 2), std::make_pair(tr("Spanish"), 3),
                            std::make_pair(tr("Italian"), 4), std::make_pair(tr("Dutch"), 5)})
  {
    m_language_combo->addItem(entry.first, entry.second);
  }

  ipl_layout->addWidget(m_skip_main_menu, 0, 0);
  ipl_layout->addWidget(new QLabel(tr("System Language:")), 1, 0);
  ipl_layout->addWidget(m_language_combo, 1, 1);

  // Device Settings
  QGroupBox* device_box = new QGroupBox(tr("Device Settings"), this);
  QGridLayout* device_layout = new QGridLayout(device_box);
  device_box->setLayout(device_layout);

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    m_slot_combos[i] = new QComboBox(device_box);
    m_slot_buttons[i] = new QPushButton(tr("..."), device_box);
    m_slot_buttons[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  // Add slot devices

  for (const auto& entry :
       {std::make_pair(tr("<Nothing>"), ExpansionInterface::EXIDEVICE_NONE),
        std::make_pair(tr("Dummy"), ExpansionInterface::EXIDEVICE_DUMMY),
        std::make_pair(tr("Memory Card"), ExpansionInterface::EXIDEVICE_MEMORYCARD),
        std::make_pair(tr("GCI Folder"), ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER),
        std::make_pair(tr("USB Gecko"), ExpansionInterface::EXIDEVICE_GECKO),
        std::make_pair(tr("Advance Game Port"), ExpansionInterface::EXIDEVICE_AGP),
        std::make_pair(tr("Microphone"), ExpansionInterface::EXIDEVICE_MIC)})
  {
    m_slot_combos[0]->addItem(entry.first, entry.second);
    m_slot_combos[1]->addItem(entry.first, entry.second);
  }

  // Add SP1 devices

  std::vector<std::pair<QString, ExpansionInterface::TEXIDevices>> sp1Entries{
      std::make_pair(tr("<Nothing>"), ExpansionInterface::EXIDEVICE_NONE),
      std::make_pair(tr("Dummy"), ExpansionInterface::EXIDEVICE_DUMMY),
      std::make_pair(tr("Broadband Adapter (TAP)"), ExpansionInterface::EXIDEVICE_ETH),
      std::make_pair(tr("Broadband Adapter (XLink Kai)"), ExpansionInterface::EXIDEVICE_ETHXLINK)};
#if defined(__APPLE__)
  sp1Entries.emplace_back(std::make_pair(tr("Broadband Adapter (tapserver)"),
                                         ExpansionInterface::EXIDEVICE_ETHTAPSERVER));
#endif
  for (const auto& entry : sp1Entries)
  {
    m_slot_combos[2]->addItem(entry.first, entry.second);
  }

  device_layout->addWidget(new QLabel(tr("Slot A:")), 0, 0);
  device_layout->addWidget(m_slot_combos[0], 0, 1);
  device_layout->addWidget(m_slot_buttons[0], 0, 2);
  device_layout->addWidget(new QLabel(tr("Slot B:")), 1, 0);
  device_layout->addWidget(m_slot_combos[1], 1, 1);
  device_layout->addWidget(m_slot_buttons[1], 1, 2);
  device_layout->addWidget(new QLabel(tr("SP1:")), 2, 0);
  device_layout->addWidget(m_slot_combos[2], 2, 1);
  device_layout->addWidget(m_slot_buttons[2], 2, 2);

#ifdef HAS_LIBMGBA
  // GBA Settings
  auto* gba_box = new QGroupBox(tr("GBA Settings"), this);
  auto* gba_layout = new QGridLayout(gba_box);
  gba_box->setLayout(gba_layout);
  int gba_row = 0;

  m_gba_threads = new QCheckBox(tr("Run GBA Cores in Dedicated Threads"));
  gba_layout->addWidget(m_gba_threads, gba_row, 0, 1, -1);
  gba_row++;

  m_gba_bios_edit = new QLineEdit();
  m_gba_browse_bios = new QPushButton(QStringLiteral("..."));
  gba_layout->addWidget(new QLabel(tr("BIOS:")), gba_row, 0);
  gba_layout->addWidget(m_gba_bios_edit, gba_row, 1);
  gba_layout->addWidget(m_gba_browse_bios, gba_row, 2);
  gba_row++;

  for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
  {
    m_gba_rom_edits[i] = new QLineEdit();
    m_gba_browse_roms[i] = new QPushButton(QStringLiteral("..."));
    gba_layout->addWidget(new QLabel(tr("Port %1 ROM:").arg(i + 1)), gba_row, 0);
    gba_layout->addWidget(m_gba_rom_edits[i], gba_row, 1);
    gba_layout->addWidget(m_gba_browse_roms[i], gba_row, 2);
    gba_row++;
  }

  m_gba_save_rom_path = new QCheckBox(tr("Save in Same Directory as the ROM"));
  gba_layout->addWidget(m_gba_save_rom_path, gba_row, 0, 1, -1);
  gba_row++;

  m_gba_saves_edit = new QLineEdit();
  m_gba_browse_saves = new QPushButton(QStringLiteral("..."));
  gba_layout->addWidget(new QLabel(tr("Saves:")), gba_row, 0);
  gba_layout->addWidget(m_gba_saves_edit, gba_row, 1);
  gba_layout->addWidget(m_gba_browse_saves, gba_row, 2);
  gba_row++;
#endif

  layout->addWidget(ipl_box);
  layout->addWidget(device_box);
#ifdef HAS_LIBMGBA
  layout->addWidget(gba_box);
#endif

  layout->addStretch();

  setLayout(layout);
}

void GameCubePane::ConnectWidgets()
{
  // IPL Settings
  connect(m_skip_main_menu, &QCheckBox::stateChanged, this, &GameCubePane::SaveSettings);
  connect(m_language_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &GameCubePane::SaveSettings);

  // Device Settings
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    connect(m_slot_combos[i], qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, i] { UpdateButton(i); });
    connect(m_slot_combos[i], qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GameCubePane::SaveSettings);
    connect(m_slot_buttons[i], &QPushButton::clicked, [this, i] { OnConfigPressed(i); });
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  connect(m_gba_threads, &QCheckBox::stateChanged, this, &GameCubePane::SaveSettings);
  connect(m_gba_bios_edit, &QLineEdit::editingFinished, this, &GameCubePane::SaveSettings);
  connect(m_gba_browse_bios, &QPushButton::clicked, this, &GameCubePane::BrowseGBABios);
  connect(m_gba_save_rom_path, &QCheckBox::stateChanged, this, &GameCubePane::SaveRomPathChanged);
  connect(m_gba_saves_edit, &QLineEdit::editingFinished, this, &GameCubePane::SaveSettings);
  connect(m_gba_browse_saves, &QPushButton::clicked, this, &GameCubePane::BrowseGBASaves);
  for (size_t i = 0; i < m_gba_browse_roms.size(); ++i)
  {
    connect(m_gba_rom_edits[i], &QLineEdit::editingFinished, this, &GameCubePane::SaveSettings);
    connect(m_gba_browse_roms[i], &QPushButton::clicked, this, [this, i] { BrowseGBARom(i); });
  }
#endif

  // Emulation State
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &GameCubePane::OnEmulationStateChanged);
  OnEmulationStateChanged();
}

void GameCubePane::OnEmulationStateChanged()
{
#ifdef HAS_LIBMGBA
  bool gba_enabled = !NetPlay::IsNetPlayRunning();
  m_gba_threads->setEnabled(gba_enabled);
  m_gba_bios_edit->setEnabled(gba_enabled);
  m_gba_browse_bios->setEnabled(gba_enabled);
  m_gba_save_rom_path->setEnabled(gba_enabled);
  m_gba_saves_edit->setEnabled(gba_enabled);
  m_gba_browse_saves->setEnabled(gba_enabled);
  for (size_t i = 0; i < m_gba_browse_roms.size(); ++i)
  {
    m_gba_rom_edits[i]->setEnabled(gba_enabled);
    m_gba_browse_roms[i]->setEnabled(gba_enabled);
  }
#endif
}

void GameCubePane::UpdateButton(int slot)
{
  const auto value = m_slot_combos[slot]->currentData().toInt();
  bool has_config = false;

  switch (slot)
  {
  case SLOT_A_INDEX:
  case SLOT_B_INDEX:
    has_config =
        (value == ExpansionInterface::EXIDEVICE_MEMORYCARD ||
         value == ExpansionInterface::EXIDEVICE_AGP || value == ExpansionInterface::EXIDEVICE_MIC);
    break;
  case SLOT_SP1_INDEX:
    has_config = (value == ExpansionInterface::EXIDEVICE_ETH ||
                  value == ExpansionInterface::EXIDEVICE_ETHXLINK);
    break;
  }

  m_slot_buttons[slot]->setEnabled(has_config);
}

void GameCubePane::OnConfigPressed(int slot)
{
  QString filter;
  bool memcard = false;

  switch (m_slot_combos[slot]->currentData().toInt())
  {
  case ExpansionInterface::EXIDEVICE_MEMORYCARD:
    filter = tr("GameCube Memory Cards (*.raw *.gcp)");
    memcard = true;
    break;
  case ExpansionInterface::EXIDEVICE_AGP:
    filter = tr("Game Boy Advance Carts (*.gba)");
    break;
  case ExpansionInterface::EXIDEVICE_MIC:
    MappingWindow(this, MappingWindow::Type::MAPPING_GC_MICROPHONE, slot).exec();
    return;
  case ExpansionInterface::EXIDEVICE_ETH:
  {
    bool ok;
    const auto new_mac = QInputDialog::getText(
        // i18n: MAC stands for Media Access Control. A MAC address uniquely identifies a network
        // interface (physical) like a serial number. "MAC" should be kept in translations.
        this, tr("Broadband Adapter MAC address"), tr("Enter new Broadband Adapter MAC address:"),
        QLineEdit::Normal, QString::fromStdString(SConfig::GetInstance().m_bba_mac), &ok);
    if (ok)
      SConfig::GetInstance().m_bba_mac = new_mac.toStdString();
    return;
  }
  case ExpansionInterface::EXIDEVICE_ETHXLINK:
  {
    bool ok;
    const auto new_dest = QInputDialog::getText(
        this, tr("Broadband Adapter (XLink Kai) Destination Address"),
        tr("Enter IP address of device running the XLink Kai Client.\nFor more information see"
           " https://www.teamxlink.co.uk/wiki/Dolphin"),
        QLineEdit::Normal, QString::fromStdString(SConfig::GetInstance().m_bba_xlink_ip), &ok);
    if (ok)
      SConfig::GetInstance().m_bba_xlink_ip = new_dest.toStdString();
    return;
  }
  default:
    qFatal("unknown settings pressed");
  }

  QString filename = QFileDialog::getSaveFileName(
      this, tr("Choose a file to open"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      filter, 0, QFileDialog::DontConfirmOverwrite);

  if (filename.isEmpty())
    return;

  QString path_abs = QFileInfo(filename).absoluteFilePath();

  // Memcard validity checks
  if (memcard)
  {
    if (File::Exists(filename.toStdString()))
    {
      auto [error_code, mc] = Memcard::GCMemcard::Open(filename.toStdString());

      if (error_code.HasCriticalErrors() || !mc || !mc->IsValid())
      {
        ModalMessageBox::critical(
            this, tr("Error"),
            tr("The file\n%1\nis either corrupted or not a GameCube memory card file.\n%2")
                .arg(filename)
                .arg(GCMemcardManager::GetErrorMessagesForErrorCode(error_code)));
        return;
      }
    }

    bool other_slot_memcard =
        m_slot_combos[slot == SLOT_A_INDEX ? SLOT_B_INDEX : SLOT_A_INDEX]->currentData().toInt() ==
        ExpansionInterface::EXIDEVICE_MEMORYCARD;
    if (other_slot_memcard)
    {
      QString path_b =
          QFileInfo(QString::fromStdString(slot == 0 ? Config::Get(Config::MAIN_MEMCARD_B_PATH) :
                                                       Config::Get(Config::MAIN_MEMCARD_A_PATH)))
              .absoluteFilePath();

      if (path_abs == path_b)
      {
        ModalMessageBox::critical(this, tr("Error"),
                                  tr("The same file can't be used in both slots."));
        return;
      }
    }
  }

  QString path_old;
  if (memcard)
  {
    path_old =
        QFileInfo(QString::fromStdString(slot == 0 ? Config::Get(Config::MAIN_MEMCARD_A_PATH) :
                                                     Config::Get(Config::MAIN_MEMCARD_B_PATH)))
            .absoluteFilePath();
  }
  else
  {
    path_old = QFileInfo(QString::fromStdString(slot == 0 ? SConfig::GetInstance().m_strGbaCartA :
                                                            SConfig::GetInstance().m_strGbaCartB))
                   .absoluteFilePath();
  }

  if (memcard)
  {
    if (slot == SLOT_A_INDEX)
    {
      Config::SetBase(Config::MAIN_MEMCARD_A_PATH, path_abs.toStdString());
    }
    else
    {
      Config::SetBase(Config::MAIN_MEMCARD_B_PATH, path_abs.toStdString());
    }
  }
  else
  {
    if (slot == SLOT_A_INDEX)
    {
      SConfig::GetInstance().m_strGbaCartA = path_abs.toStdString();
    }
    else
    {
      SConfig::GetInstance().m_strGbaCartB = path_abs.toStdString();
    }
  }

  if (Core::IsRunning() && path_abs != path_old)
  {
    ExpansionInterface::ChangeDevice(
        // SlotB is on channel 1, slotA and SP1 are on 0
        slot,
        // The device enum to change to
        memcard ? ExpansionInterface::EXIDEVICE_MEMORYCARD : ExpansionInterface::EXIDEVICE_AGP,
        // SP1 is device 2, slots are device 0
        0);
  }
}

void GameCubePane::BrowseGBABios()
{
  QString file = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this, tr("Select GBA BIOS"), QString::fromStdString(File::GetUserPath(F_GBABIOS_IDX)),
      tr("All Files (*)")));
  if (!file.isEmpty())
  {
    m_gba_bios_edit->setText(file);
    SaveSettings();
  }
}

void GameCubePane::BrowseGBARom(size_t index)
{
  QString file = QString::fromStdString(GetOpenGBARom({}));
  if (!file.isEmpty())
  {
    m_gba_rom_edits[index]->setText(file);
    SaveSettings();
  }
}

void GameCubePane::SaveRomPathChanged()
{
  m_gba_saves_edit->setEnabled(!m_gba_save_rom_path->isChecked());
  m_gba_browse_saves->setEnabled(!m_gba_save_rom_path->isChecked());
  SaveSettings();
}

void GameCubePane::BrowseGBASaves()
{
  QString dir = QDir::toNativeSeparators(
      QFileDialog::getExistingDirectory(this, tr("Select GBA Saves Path"),
                                        QString::fromStdString(File::GetUserPath(D_GBASAVES_IDX))));
  if (!dir.isEmpty())
  {
    m_gba_saves_edit->setText(dir);
    SaveSettings();
  }
}

void GameCubePane::LoadSettings()
{
  const SConfig& params = SConfig::GetInstance();

  // IPL Settings
  m_skip_main_menu->setChecked(params.bHLE_BS2);
  m_language_combo->setCurrentIndex(m_language_combo->findData(params.SelectedLanguage));

  bool have_menu = false;

  for (const std::string& dir : {USA_DIR, JAP_DIR, EUR_DIR})
  {
    const auto path = DIR_SEP + dir + DIR_SEP GC_IPL;
    if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + path) ||
        File::Exists(File::GetSysDirectory() + GC_SYS_DIR + path))
    {
      have_menu = true;
      break;
    }
  }

  m_skip_main_menu->setEnabled(have_menu);
  m_skip_main_menu->setToolTip(have_menu ? QString{} :
                                           tr("Put Main Menu roms in User/GC/{region}."));

  // Device Settings

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    QSignalBlocker blocker(m_slot_combos[i]);
    m_slot_combos[i]->setCurrentIndex(
        m_slot_combos[i]->findData(SConfig::GetInstance().m_EXIDevice[i]));
    UpdateButton(i);
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  m_gba_threads->setChecked(Config::Get(Config::MAIN_GBA_THREADS));
  m_gba_bios_edit->setText(QString::fromStdString(File::GetUserPath(F_GBABIOS_IDX)));
  m_gba_save_rom_path->setChecked(Config::Get(Config::MAIN_GBA_SAVES_IN_ROM_PATH));
  m_gba_saves_edit->setText(QString::fromStdString(File::GetUserPath(D_GBASAVES_IDX)));
  for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
    m_gba_rom_edits[i]->setText(QString::fromStdString(Config::Get(Config::MAIN_GBA_ROM_PATHS[i])));
#endif
}

void GameCubePane::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  SConfig& params = SConfig::GetInstance();

  // IPL Settings
  params.bHLE_BS2 = m_skip_main_menu->isChecked();
  Config::SetBaseOrCurrent(Config::MAIN_SKIP_IPL, m_skip_main_menu->isChecked());
  params.SelectedLanguage = m_language_combo->currentData().toInt();
  Config::SetBaseOrCurrent(Config::MAIN_GC_LANGUAGE, m_language_combo->currentData().toInt());

  // Device Settings
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    const auto dev = ExpansionInterface::TEXIDevices(m_slot_combos[i]->currentData().toInt());

    if (Core::IsRunning() && SConfig::GetInstance().m_EXIDevice[i] != dev)
    {
      ExpansionInterface::ChangeDevice(
          // SlotB is on channel 1, slotA and SP1 are on 0
          (i == 1) ? 1 : 0,
          // The device enum to change to
          dev,
          // SP1 is device 2, slots are device 0
          (i == 2) ? 2 : 0);
    }

    SConfig::GetInstance().m_EXIDevice[i] = dev;
    switch (i)
    {
    case SLOT_A_INDEX:
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A, dev);
      break;
    case SLOT_B_INDEX:
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, dev);
      break;
    case SLOT_SP1_INDEX:
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1, dev);
      break;
    }
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  if (!NetPlay::IsNetPlayRunning())
  {
    Config::SetBaseOrCurrent(Config::MAIN_GBA_THREADS, m_gba_threads->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_GBA_BIOS_PATH, m_gba_bios_edit->text().toStdString());
    Config::SetBaseOrCurrent(Config::MAIN_GBA_SAVES_IN_ROM_PATH, m_gba_save_rom_path->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_GBA_SAVES_PATH, m_gba_saves_edit->text().toStdString());
    File::SetUserPath(F_GBABIOS_IDX, Config::Get(Config::MAIN_GBA_BIOS_PATH));
    File::SetUserPath(D_GBASAVES_IDX, Config::Get(Config::MAIN_GBA_SAVES_PATH));
    for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
    {
      Config::SetBaseOrCurrent(Config::MAIN_GBA_ROM_PATHS[i],
                               m_gba_rom_edits[i]->text().toStdString());
    }

    auto server = Settings::Instance().GetNetPlayServer();
    if (server)
      server->SetGBAConfig(server->GetGBAConfig(), true);
  }
#endif

  LoadSettings();
}

std::string GameCubePane::GetOpenGBARom(std::string_view title)
{
  QString caption = tr("Select GBA ROM");
  if (!title.empty())
    caption += QStringLiteral(": %1").arg(QString::fromStdString(std::string(title)));
  return QDir::toNativeSeparators(
             QFileDialog::getOpenFileName(
                 nullptr, caption, QString(),
                 tr("Game Boy Advance ROMs (*.gba *.gbc *.gb *.7z *.zip *.agb *.mb *.rom *.bin);;"
                    "All Files (*)")))
      .toStdString();
}
