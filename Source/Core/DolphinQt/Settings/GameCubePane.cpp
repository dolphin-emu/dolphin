// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/GameCubePane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>
#include <utility>

#include "Common/Assert.h"
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
#include "Core/System.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/GCMemcardManager.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/BroadbandAdapterSettingsDialog.h"

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
  using ExpansionInterface::EXIDeviceType;

  QVBoxLayout* layout = new QVBoxLayout(this);

  // IPL Settings
  QGroupBox* ipl_box = new QGroupBox(tr("IPL Settings"), this);
  QVBoxLayout* ipl_box_layout = new QVBoxLayout(ipl_box);
  ipl_box->setLayout(ipl_box_layout);

  m_skip_main_menu = new QCheckBox(tr("Skip Main Menu"), ipl_box);
  ipl_box_layout->addWidget(m_skip_main_menu);

  QFormLayout* ipl_language_layout = new QFormLayout;
  ipl_language_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  ipl_language_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  ipl_box_layout->addLayout(ipl_language_layout);

  m_language_combo = new QComboBox(ipl_box);
  m_language_combo->setCurrentIndex(-1);
  ipl_language_layout->addRow(tr("System Language:"), m_language_combo);

  // Add languages
  for (const auto& entry : {std::make_pair(tr("English"), 0), std::make_pair(tr("German"), 1),
                            std::make_pair(tr("French"), 2), std::make_pair(tr("Spanish"), 3),
                            std::make_pair(tr("Italian"), 4), std::make_pair(tr("Dutch"), 5)})
  {
    m_language_combo->addItem(entry.first, entry.second);
  }

  // Device Settings
  QGroupBox* device_box = new QGroupBox(tr("Device Settings"), this);
  QGridLayout* device_layout = new QGridLayout(device_box);
  device_box->setLayout(device_layout);

  for (ExpansionInterface::Slot slot : ExpansionInterface::SLOTS)
  {
    m_slot_combos[slot] = new QComboBox(device_box);
    m_slot_combos[slot]->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_slot_buttons[slot] = new NonDefaultQPushButton(tr("..."), device_box);
    m_slot_buttons[slot]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    m_memcard_path_layouts[slot] = new QHBoxLayout();
    m_memcard_path_labels[slot] = new QLabel(tr("Memory Card Path:"));
    m_memcard_paths[slot] = new QLineEdit();
    m_memcard_path_layouts[slot]->addWidget(m_memcard_path_labels[slot]);
    m_memcard_path_layouts[slot]->addWidget(m_memcard_paths[slot]);

    m_agp_path_layouts[slot] = new QHBoxLayout();
    m_agp_path_labels[slot] = new QLabel(tr("GBA Cartridge Path:"));
    m_agp_paths[slot] = new QLineEdit();
    m_agp_path_layouts[slot]->addWidget(m_agp_path_labels[slot]);
    m_agp_path_layouts[slot]->addWidget(m_agp_paths[slot]);

    m_gci_path_layouts[slot] = new QVBoxLayout();
    m_gci_path_labels[slot] = new QLabel(tr("GCI Folder Path:"));
    m_gci_override_labels[slot] =
        new QLabel(tr("Warning: A GCI folder override path is currently configured for this slot. "
                      "Adjusting the GCI path here will have no effect."));
    m_gci_override_labels[slot]->setHidden(true);
    m_gci_override_labels[slot]->setWordWrap(true);
    m_gci_paths[slot] = new QLineEdit();
    auto* hlayout = new QHBoxLayout();
    hlayout->addWidget(m_gci_path_labels[slot]);
    hlayout->addWidget(m_gci_paths[slot]);
    m_gci_path_layouts[slot]->addWidget(m_gci_override_labels[slot]);
    m_gci_path_layouts[slot]->addLayout(hlayout);
  }

  // Add slot devices
  for (const auto device : {EXIDeviceType::None, EXIDeviceType::Dummy, EXIDeviceType::MemoryCard,
                            EXIDeviceType::MemoryCardFolder, EXIDeviceType::Gecko,
                            EXIDeviceType::AGP, EXIDeviceType::Microphone})
  {
    const QString name = tr(fmt::format("{:n}", device).c_str());
    const int value = static_cast<int>(device);
    m_slot_combos[ExpansionInterface::Slot::A]->addItem(name, value);
    m_slot_combos[ExpansionInterface::Slot::B]->addItem(name, value);
  }

  // Add SP1 devices
  for (const auto device : {
           EXIDeviceType::None,
           EXIDeviceType::Dummy,
           EXIDeviceType::Ethernet,
           EXIDeviceType::EthernetXLink,
#ifdef __APPLE__
           EXIDeviceType::EthernetTapServer,
#endif
           EXIDeviceType::EthernetBuiltIn,
       })
  {
    m_slot_combos[ExpansionInterface::Slot::SP1]->addItem(tr(fmt::format("{:n}", device).c_str()),
                                                          static_cast<int>(device));
  }

  {
    int row = 0;
    device_layout->addWidget(new QLabel(tr("Slot A:")), row, 0);
    device_layout->addWidget(m_slot_combos[ExpansionInterface::Slot::A], row, 1);
    device_layout->addWidget(m_slot_buttons[ExpansionInterface::Slot::A], row, 2);

    ++row;
    device_layout->addLayout(m_memcard_path_layouts[ExpansionInterface::Slot::A], row, 0, 1, 3);

    ++row;
    device_layout->addLayout(m_agp_path_layouts[ExpansionInterface::Slot::A], row, 0, 1, 3);

    ++row;
    device_layout->addLayout(m_gci_path_layouts[ExpansionInterface::Slot::A], row, 0, 1, 3);

    ++row;
    device_layout->addWidget(new QLabel(tr("Slot B:")), row, 0);
    device_layout->addWidget(m_slot_combos[ExpansionInterface::Slot::B], row, 1);
    device_layout->addWidget(m_slot_buttons[ExpansionInterface::Slot::B], row, 2);

    ++row;
    device_layout->addLayout(m_memcard_path_layouts[ExpansionInterface::Slot::B], row, 0, 1, 3);

    ++row;
    device_layout->addLayout(m_agp_path_layouts[ExpansionInterface::Slot::B], row, 0, 1, 3);

    ++row;
    device_layout->addLayout(m_gci_path_layouts[ExpansionInterface::Slot::B], row, 0, 1, 3);

    ++row;
    device_layout->addWidget(new QLabel(tr("SP1:")), row, 0);
    device_layout->addWidget(m_slot_combos[ExpansionInterface::Slot::SP1], row, 1);
    device_layout->addWidget(m_slot_buttons[ExpansionInterface::Slot::SP1], row, 2);
  }

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
  m_gba_browse_bios = new NonDefaultQPushButton(QStringLiteral("..."));
  gba_layout->addWidget(new QLabel(tr("BIOS:")), gba_row, 0);
  gba_layout->addWidget(m_gba_bios_edit, gba_row, 1);
  gba_layout->addWidget(m_gba_browse_bios, gba_row, 2);
  gba_row++;

  for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
  {
    m_gba_rom_edits[i] = new QLineEdit();
    m_gba_browse_roms[i] = new NonDefaultQPushButton(QStringLiteral("..."));
    gba_layout->addWidget(new QLabel(tr("Port %1 ROM:").arg(i + 1)), gba_row, 0);
    gba_layout->addWidget(m_gba_rom_edits[i], gba_row, 1);
    gba_layout->addWidget(m_gba_browse_roms[i], gba_row, 2);
    gba_row++;
  }

  m_gba_save_rom_path = new QCheckBox(tr("Save in Same Directory as the ROM"));
  gba_layout->addWidget(m_gba_save_rom_path, gba_row, 0, 1, -1);
  gba_row++;

  m_gba_saves_edit = new QLineEdit();
  m_gba_browse_saves = new NonDefaultQPushButton(QStringLiteral("..."));
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
  connect(m_language_combo, &QComboBox::currentIndexChanged, this, &GameCubePane::SaveSettings);

  // Device Settings
  for (ExpansionInterface::Slot slot : ExpansionInterface::SLOTS)
  {
    connect(m_slot_combos[slot], &QComboBox::currentIndexChanged, this,
            [this, slot] { UpdateButton(slot); });
    connect(m_slot_combos[slot], &QComboBox::currentIndexChanged, this,
            &GameCubePane::SaveSettings);
    connect(m_slot_buttons[slot], &QPushButton::clicked, [this, slot] { OnConfigPressed(slot); });
  }

  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    connect(m_memcard_paths[slot], &QLineEdit::editingFinished, [this, slot] {
      // revert path change on failure
      if (!SetMemcard(slot, m_memcard_paths[slot]->text()))
        LoadSettings();
    });
    connect(m_agp_paths[slot], &QLineEdit::editingFinished,
            [this, slot] { SetAGPRom(slot, m_agp_paths[slot]->text()); });
    connect(m_gci_paths[slot], &QLineEdit::editingFinished, [this, slot] {
      // revert path change on failure
      if (!SetGCIFolder(slot, m_gci_paths[slot]->text()))
        LoadSettings();
    });
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

void GameCubePane::UpdateButton(ExpansionInterface::Slot slot)
{
  const auto device =
      static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());
  bool has_config = false;

  switch (slot)
  {
  case ExpansionInterface::Slot::A:
  case ExpansionInterface::Slot::B:
  {
    has_config = (device == ExpansionInterface::EXIDeviceType::MemoryCard ||
                  device == ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
                  device == ExpansionInterface::EXIDeviceType::AGP ||
                  device == ExpansionInterface::EXIDeviceType::Microphone);
    const bool hide_memory_card = device != ExpansionInterface::EXIDeviceType::MemoryCard ||
                                  Config::IsDefaultMemcardPathConfigured(slot);
    const bool hide_gci_path = device != ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
                               Config::IsDefaultGCIFolderPathConfigured(slot);
    m_memcard_path_labels[slot]->setHidden(hide_memory_card);
    m_memcard_paths[slot]->setHidden(hide_memory_card);
    m_agp_path_labels[slot]->setHidden(device != ExpansionInterface::EXIDeviceType::AGP);
    m_agp_paths[slot]->setHidden(device != ExpansionInterface::EXIDeviceType::AGP);
    m_gci_path_labels[slot]->setHidden(hide_gci_path);
    m_gci_paths[slot]->setHidden(hide_gci_path);

    // In the years before we introduced the GCI folder configuration paths it has become somewhat
    // popular to use the GCI override path instead. Check if this is the case and display a warning
    // if it is set, so users aren't confused why configuring the normal GCI path doesn't do
    // anything.
    m_gci_override_labels[slot]->setHidden(
        device != ExpansionInterface::EXIDeviceType::MemoryCardFolder ||
        Config::Get(Config::GetInfoForGCIPathOverride(slot)).empty());

    break;
  }
  case ExpansionInterface::Slot::SP1:
    has_config = (device == ExpansionInterface::EXIDeviceType::Ethernet ||
                  device == ExpansionInterface::EXIDeviceType::EthernetXLink ||
                  device == ExpansionInterface::EXIDeviceType::EthernetBuiltIn);
    break;
  }

  m_slot_buttons[slot]->setEnabled(has_config);
}

void GameCubePane::OnConfigPressed(ExpansionInterface::Slot slot)
{
  const ExpansionInterface::EXIDeviceType device =
      static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());

  switch (device)
  {
  case ExpansionInterface::EXIDeviceType::MemoryCard:
    BrowseMemcard(slot);
    return;
  case ExpansionInterface::EXIDeviceType::MemoryCardFolder:
    BrowseGCIFolder(slot);
    return;
  case ExpansionInterface::EXIDeviceType::AGP:
    BrowseAGPRom(slot);
    return;
  case ExpansionInterface::EXIDeviceType::Microphone:
  {
    // TODO: convert MappingWindow to use Slot?
    MappingWindow dialog(this, MappingWindow::Type::MAPPING_GC_MICROPHONE, static_cast<int>(slot));
    SetQWidgetWindowDecorations(&dialog);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::Ethernet:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::Ethernet);
    SetQWidgetWindowDecorations(&dialog);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::EthernetXLink:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::XLinkKai);
    SetQWidgetWindowDecorations(&dialog);
    dialog.exec();
    return;
  }
  case ExpansionInterface::EXIDeviceType::EthernetBuiltIn:
  {
    BroadbandAdapterSettingsDialog dialog(this, BroadbandAdapterSettingsDialog::Type::BuiltIn);
    SetQWidgetWindowDecorations(&dialog);
    dialog.exec();
    return;
  }
  default:
    PanicAlertFmt("Unknown settings pressed for {}", device);
    return;
  }
}

void GameCubePane::BrowseMemcard(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  const QString filename = DolphinFileDialog::getSaveFileName(
      this, tr("Choose a file to open or create"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)"), nullptr, QFileDialog::DontConfirmOverwrite);

  if (!filename.isEmpty())
    SetMemcard(slot, filename);
}

bool GameCubePane::SetMemcard(ExpansionInterface::Slot slot, const QString& filename)
{
  if (filename.isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Cannot set memory card to an empty path."));
    return false;
  }

  const std::string raw_path =
      WithUnifiedPathSeparators(QFileInfo(filename).absoluteFilePath().toStdString());

  // Figure out if the user selected a card that has a valid region specifier in the filename.
  const std::string jp_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::NTSC_J);
  const std::string us_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::NTSC_U);
  const std::string eu_path = Config::GetMemcardPath(raw_path, slot, DiscIO::Region::PAL);
  const bool raw_path_valid = raw_path == jp_path || raw_path == us_path || raw_path == eu_path;

  if (!raw_path_valid)
  {
    // TODO: We could try to autodetect the card region here and offer automatic renaming.
    ModalMessageBox::critical(this, tr("Error"),
                              tr("The filename %1 does not conform to Dolphin's region code format "
                                 "for memory cards. Please rename this file to either %2, %3, or "
                                 "%4, matching the region of the save files that are on it.")
                                  .arg(QString::fromStdString(PathToFileName(raw_path)))
                                  .arg(QString::fromStdString(PathToFileName(us_path)))
                                  .arg(QString::fromStdString(PathToFileName(eu_path)))
                                  .arg(QString::fromStdString(PathToFileName(jp_path))));
    return false;
  }

  // Memcard validity checks
  for (const std::string& path : {jp_path, us_path, eu_path})
  {
    if (File::Exists(path))
    {
      auto [error_code, mc] = Memcard::GCMemcard::Open(path);

      if (error_code.HasCriticalErrors() || !mc || !mc->IsValid())
      {
        ModalMessageBox::critical(
            this, tr("Error"),
            tr("The file\n%1\nis either corrupted or not a GameCube memory card file.\n%2")
                .arg(QString::fromStdString(path))
                .arg(GCMemcardManager::GetErrorMessagesForErrorCode(error_code)));
        return false;
      }
    }
  }

  // Check if the other slot has the same memory card configured and refuse to use this card if so.
  // The EU path is compared here, but it doesn't actually matter which one we compare since they
  // follow a known pattern, so if the EU path matches the other match too and vice-versa.
  for (ExpansionInterface::Slot other_slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    if (other_slot == slot)
      continue;

    const std::string other_eu_path = Config::GetMemcardPath(other_slot, DiscIO::Region::PAL);
    if (eu_path == other_eu_path)
    {
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("The same file can't be used in multiple slots; it is already used by %1.")
              .arg(QString::fromStdString(fmt::to_string(other_slot))));
      return false;
    }
  }

  const std::string old_eu_path = Config::GetMemcardPath(slot, DiscIO::Region::PAL);
  Config::SetBase(Config::GetInfoForMemcardPath(slot), raw_path);

  if (Core::IsRunning())
  {
    // If emulation is running and the new card is different from the old one, notify the system to
    // eject the old and insert the new card.
    // TODO: This should probably done by a config change callback instead.
    if (eu_path != old_eu_path)
    {
      // ChangeDevice unplugs the device for 1 second, which means that games should notice that
      // the path has changed and thus the memory card contents have changed
      Core::System::GetInstance().GetExpansionInterface().ChangeDevice(
          slot, ExpansionInterface::EXIDeviceType::MemoryCard);
    }
  }

  LoadSettings();
  return true;
}

void GameCubePane::BrowseGCIFolder(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  const QString path = DolphinFileDialog::getExistingDirectory(
      this, tr("Choose the GCI base folder"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)));

  if (!path.isEmpty())
    SetGCIFolder(slot, path);
}

bool GameCubePane::SetGCIFolder(ExpansionInterface::Slot slot, const QString& path)
{
  if (path.isEmpty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Cannot set GCI folder to an empty path."));
    return false;
  }

  std::string raw_path =
      WithUnifiedPathSeparators(QFileInfo(path).absoluteFilePath().toStdString());
  while (raw_path.ends_with('/'))
    raw_path.pop_back();

  // The user might be attempting to reset this path to its default, check for this.
  const std::string default_jp_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::NTSC_J);
  const std::string default_us_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::NTSC_U);
  const std::string default_eu_path = Config::GetGCIFolderPath("", slot, DiscIO::Region::PAL);
  const bool is_default_path =
      raw_path == default_jp_path || raw_path == default_us_path || raw_path == default_eu_path;

  bool path_changed;
  if (is_default_path)
  {
    // Reset to default.
    // Note that this does not need to check if the same card is in the other slot, because that's
    // impossible given our constraints for folder names.
    raw_path = "";
    path_changed = !Config::IsDefaultGCIFolderPathConfigured(slot);
  }
  else
  {
    // Figure out if the user selected a folder that ends in a valid region specifier.
    const std::string jp_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::NTSC_J);
    const std::string us_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::NTSC_U);
    const std::string eu_path = Config::GetGCIFolderPath(raw_path, slot, DiscIO::Region::PAL);
    const bool raw_path_valid = raw_path == jp_path || raw_path == us_path || raw_path == eu_path;

    if (!raw_path_valid)
    {
      // TODO: We could try to autodetect the card region here and offer automatic renaming.
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("The folder %1 does not conform to Dolphin's region code format "
             "for GCI folders. Please rename this folder to either %2, %3, or "
             "%4, matching the region of the save files that are in it.")
              .arg(QString::fromStdString(PathToFileName(raw_path)))
              .arg(QString::fromStdString(PathToFileName(us_path)))
              .arg(QString::fromStdString(PathToFileName(eu_path)))
              .arg(QString::fromStdString(PathToFileName(jp_path))));
      return false;
    }

    // Check if the other slot has the same folder configured and refuse to use this folder if so.
    // The EU path is compared here, but it doesn't actually matter which one we compare since they
    // follow a known pattern, so if the EU path matches the other match too and vice-versa.
    for (ExpansionInterface::Slot other_slot : ExpansionInterface::MEMCARD_SLOTS)
    {
      if (other_slot == slot)
        continue;

      const std::string other_eu_path = Config::GetGCIFolderPath(other_slot, DiscIO::Region::PAL);
      if (eu_path == other_eu_path)
      {
        ModalMessageBox::critical(
            this, tr("Error"),
            tr("The same folder can't be used in multiple slots; it is already used by %1.")
                .arg(QString::fromStdString(fmt::to_string(other_slot))));
        return false;
      }
    }

    path_changed = eu_path != Config::GetGCIFolderPath(slot, DiscIO::Region::PAL);
  }

  Config::SetBase(Config::GetInfoForGCIPath(slot), raw_path);

  if (Core::IsRunning())
  {
    // If emulation is running and the new card is different from the old one, notify the system to
    // eject the old and insert the new card.
    // TODO: This should probably be done by a config change callback instead.
    if (path_changed)
    {
      // ChangeDevice unplugs the device for 1 second, which means that games should notice that
      // the path has changed and thus the memory card contents have changed
      Core::System::GetInstance().GetExpansionInterface().ChangeDevice(
          slot, ExpansionInterface::EXIDeviceType::MemoryCardFolder);
    }
  }

  LoadSettings();
  return true;
}

void GameCubePane::BrowseAGPRom(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));

  QString filename = DolphinFileDialog::getSaveFileName(
      this, tr("Choose a file to open"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("Game Boy Advance Carts (*.gba)"), nullptr, QFileDialog::DontConfirmOverwrite);

  if (!filename.isEmpty())
    SetAGPRom(slot, filename);
}

void GameCubePane::SetAGPRom(ExpansionInterface::Slot slot, const QString& filename)
{
  QString path_abs = filename.isEmpty() ? QString() : QFileInfo(filename).absoluteFilePath();

  QString path_old =
      QFileInfo(QString::fromStdString(Config::Get(Config::GetInfoForAGPCartPath(slot))))
          .absoluteFilePath();

  Config::SetBase(Config::GetInfoForAGPCartPath(slot), path_abs.toStdString());

  if (Core::IsRunning() && path_abs != path_old)
  {
    // ChangeDevice unplugs the device for 1 second.  For an actual AGP, you can remove the
    // cartridge without unplugging it, and it's not clear if the AGP software actually notices
    // that it's been unplugged or the cartridge has changed, but this was done for memcards so
    // we might as well do it for the AGP too.
    Core::System::GetInstance().GetExpansionInterface().ChangeDevice(
        slot, ExpansionInterface::EXIDeviceType::AGP);
  }

  LoadSettings();
}

void GameCubePane::BrowseGBABios()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
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
  QString dir = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select GBA Saves Path"),
      QString::fromStdString(File::GetUserPath(D_GBASAVES_IDX))));
  if (!dir.isEmpty())
  {
    m_gba_saves_edit->setText(dir);
    SaveSettings();
  }
}

void GameCubePane::LoadSettings()
{
  // IPL Settings
  SignalBlocking(m_skip_main_menu)->setChecked(Config::Get(Config::MAIN_SKIP_IPL));
  SignalBlocking(m_language_combo)
      ->setCurrentIndex(m_language_combo->findData(Config::Get(Config::MAIN_GC_LANGUAGE)));

  bool have_menu = false;

  for (const std::string dir : {USA_DIR, JAP_DIR, EUR_DIR})
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
  m_skip_main_menu->setToolTip(have_menu ? QString{} : tr("Put IPL ROMs in User/GC/<region>."));

  // Device Settings
  for (ExpansionInterface::Slot slot : ExpansionInterface::SLOTS)
  {
    const ExpansionInterface::EXIDeviceType exi_device =
        Config::Get(Config::GetInfoForEXIDevice(slot));
    SignalBlocking(m_slot_combos[slot])
        ->setCurrentIndex(m_slot_combos[slot]->findData(static_cast<int>(exi_device)));
    UpdateButton(slot);
  }

  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    SignalBlocking(m_memcard_paths[slot])
        ->setText(QString::fromStdString(Config::GetMemcardPath(slot, std::nullopt)));
    SignalBlocking(m_agp_paths[slot])
        ->setText(QString::fromStdString(Config::Get(Config::GetInfoForAGPCartPath(slot))));
    SignalBlocking(m_gci_paths[slot])
        ->setText(QString::fromStdString(Config::GetGCIFolderPath(slot, std::nullopt)));
  }

#ifdef HAS_LIBMGBA
  // GBA Settings
  SignalBlocking(m_gba_threads)->setChecked(Config::Get(Config::MAIN_GBA_THREADS));
  SignalBlocking(m_gba_bios_edit)
      ->setText(QString::fromStdString(File::GetUserPath(F_GBABIOS_IDX)));
  SignalBlocking(m_gba_save_rom_path)->setChecked(Config::Get(Config::MAIN_GBA_SAVES_IN_ROM_PATH));
  SignalBlocking(m_gba_saves_edit)
      ->setText(QString::fromStdString(File::GetUserPath(D_GBASAVES_IDX)));
  for (size_t i = 0; i < m_gba_rom_edits.size(); ++i)
  {
    SignalBlocking(m_gba_rom_edits[i])
        ->setText(QString::fromStdString(Config::Get(Config::MAIN_GBA_ROM_PATHS[i])));
  }
#endif
}

void GameCubePane::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  // IPL Settings
  Config::SetBaseOrCurrent(Config::MAIN_SKIP_IPL, m_skip_main_menu->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_GC_LANGUAGE, m_language_combo->currentData().toInt());

  // Device Settings
  for (ExpansionInterface::Slot slot : ExpansionInterface::SLOTS)
  {
    const auto dev =
        static_cast<ExpansionInterface::EXIDeviceType>(m_slot_combos[slot]->currentData().toInt());
    const ExpansionInterface::EXIDeviceType current_exi_device =
        Config::Get(Config::GetInfoForEXIDevice(slot));

    if (Core::IsRunning() && current_exi_device != dev)
    {
      Core::System::GetInstance().GetExpansionInterface().ChangeDevice(slot, dev);
    }

    Config::SetBaseOrCurrent(Config::GetInfoForEXIDevice(slot), dev);
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
             DolphinFileDialog::getOpenFileName(
                 nullptr, caption, QString(),
                 tr("Game Boy Advance ROMs (*.gba *.gbc *.gb *.7z *.zip *.agb *.mb *.rom *.bin);;"
                    "All Files (*)")))
      .toStdString();
}
