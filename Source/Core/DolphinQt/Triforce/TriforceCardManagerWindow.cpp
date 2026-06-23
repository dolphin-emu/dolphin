// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Triforce/TriforceCardManagerWindow.h"

#include <optional>

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_DeviceAMBaseboard.h"
#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

namespace
{

enum class CardGameFamily
{
  VirtuaStriker = 0,
  GekitouProYakyuu = 1,
  KeyOfAvalon = 2,
};

constexpr std::size_t SLOT_COUNT = 2;

CardGameFamily GetStoredGameFamily()
{
  return static_cast<CardGameFamily>(Settings::GetQSettings()
                                         .value(QStringLiteral("triforcecardmanager/family"),
                                                static_cast<int>(CardGameFamily::VirtuaStriker))
                                         .toInt());
}

void SetStoredGameFamily(CardGameFamily family)
{
  Settings::GetQSettings().setValue(QStringLiteral("triforcecardmanager/family"),
                                    static_cast<int>(family));
}

QString GetFamilyKey(CardGameFamily family)
{
  switch (family)
  {
  case CardGameFamily::VirtuaStriker:
    return QStringLiteral("VirtuaStriker");
  case CardGameFamily::GekitouProYakyuu:
    return QStringLiteral("GekitouProYakyuu");
  case CardGameFamily::KeyOfAvalon:
    return QStringLiteral("KeyOfAvalon");
  }

  return QStringLiteral("VirtuaStriker");
}

QString GetFamilyDisplayName(CardGameFamily family)
{
  switch (family)
  {
  case CardGameFamily::VirtuaStriker:
    return TriforceCardManagerWindow::tr("Virtua Striker");
  case CardGameFamily::GekitouProYakyuu:
    return TriforceCardManagerWindow::tr("Gekitou Pro Yakyuu");
  case CardGameFamily::KeyOfAvalon:
    return TriforceCardManagerWindow::tr("Key of Avalon");
  }

  return TriforceCardManagerWindow::tr("Virtua Striker");
}

std::optional<CardGameFamily> GetCardGameFamily(AMMediaboard::GameType game_type)
{
  switch (game_type)
  {
  case AMMediaboard::VirtuaStriker4:
  case AMMediaboard::VirtuaStriker4_2006:
    return CardGameFamily::VirtuaStriker;
  case AMMediaboard::GekitouProYakyuu:
    return CardGameFamily::GekitouProYakyuu;
  case AMMediaboard::KeyOfAvalon:
    return CardGameFamily::KeyOfAvalon;
  default:
    return std::nullopt;
  }
}

AMMediaboard::GameType GetRepresentativeGameType(CardGameFamily family)
{
  switch (family)
  {
  case CardGameFamily::VirtuaStriker:
    return AMMediaboard::VirtuaStriker4;
  case CardGameFamily::GekitouProYakyuu:
    return AMMediaboard::GekitouProYakyuu;
  case CardGameFamily::KeyOfAvalon:
    return AMMediaboard::KeyOfAvalon;
  }

  return AMMediaboard::VirtuaStriker4;
}

u32 GetSlotCount(CardGameFamily family)
{
  switch (family)
  {
  case CardGameFamily::VirtuaStriker:
  case CardGameFamily::GekitouProYakyuu:
    return 2;
  case CardGameFamily::KeyOfAvalon:
    return 1;
  }

  return 2;
}

QString GetLibraryPath(CardGameFamily family)
{
  return QString::fromStdString(File::GetUserPath(D_TRIUSER_IDX) + "Cards" DIR_SEP) +
         GetFamilyKey(family) + QLatin1Char('/');
}

QString GetStoredSlotKey(CardGameFamily family, u32 slot_index)
{
  return QStringLiteral("triforcecardmanager/%1/slot%2")
      .arg(GetFamilyKey(family))
      .arg(slot_index + 1);
}

std::optional<QString> GetStoredSelectedPath(CardGameFamily family, u32 slot_index)
{
  const QString key = GetStoredSlotKey(family, slot_index);
  QSettings& settings = Settings::GetQSettings();
  if (!settings.contains(key))
    return std::nullopt;

  return settings.value(key).toString();
}

void SetStoredSelectedPath(CardGameFamily family, u32 slot_index,
                           const std::optional<QString>& path)
{
  const QString key = GetStoredSlotKey(family, slot_index);
  QSettings& settings = Settings::GetQSettings();
  if (path.has_value())
    settings.setValue(key, *path);
  else
    settings.remove(key);
}

SerialInterface::CSIDevice_AMBaseboard* GetAMBaseboard(Core::System& system)
{
  auto& serial_interface = system.GetSerialInterface();

  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    if (serial_interface.GetDeviceType(i) != SerialInterface::SIDEVICE_AM_BASEBOARD)
      continue;

    return static_cast<SerialInterface::CSIDevice_AMBaseboard*>(serial_interface.GetDevice(i));
  }

  return nullptr;
}

QString GetStatusText(const SerialInterface::CSIDevice_AMBaseboard::ICCardSlotStatus& slot_status)
{
  if (slot_status.is_ejecting)
    return TriforceCardManagerWindow::tr("Ejecting");
  if (slot_status.is_present)
    return TriforceCardManagerWindow::tr("Inserted");
  if (slot_status.wants_inserted)
    return TriforceCardManagerWindow::tr("Queued for insert");
  if (slot_status.selected_card_path.empty())
    return TriforceCardManagerWindow::tr("No card selected");
  return TriforceCardManagerWindow::tr("Pulled");
}

void RestoreStoredCardsForRunningGameImpl(Core::System& system)
{
  if (Core::GetState(system) != Core::State::Running &&
      Core::GetState(system) != Core::State::Paused)
    return;

  const auto active_family =
      GetCardGameFamily(static_cast<AMMediaboard::GameType>(AMMediaboard::GetGameType()));
  if (!active_family)
    return;

  const Core::CPUThreadGuard guard(system);
  auto* const baseboard = GetAMBaseboard(system);
  if (!baseboard || baseboard->GetICCardSlotCount() == 0)
    return;

  SetStoredGameFamily(*active_family);

  const u32 slot_count = baseboard->GetICCardSlotCount();
  std::array<std::optional<QString>, SLOT_COUNT> stored_paths;
  for (u32 i = 0; i < slot_count; ++i)
    stored_paths[i] = GetStoredSelectedPath(*active_family, i);

  for (u32 i = 0; i < slot_count; ++i)
  {
    const auto status = baseboard->GetICCardSlotStatus(i);
    if (!stored_paths[i].has_value() || !status.selected_card_path.empty())
      continue;

    if (stored_paths[i]->isEmpty())
    {
      baseboard->SetICCardSlotPath(i, "");
      baseboard->SetICCardSlotInserted(i, false);
      continue;
    }

    if (slot_count > 1 && stored_paths[i] == stored_paths[1 - i])
      continue;

    baseboard->SetICCardSlotPath(i, stored_paths[i]->toStdString());
  }
}

}  // namespace

TriforceCardManagerWindow::TriforceCardManagerWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Triforce Card Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QStringLiteral("triforce_card_manager"));
  setMinimumSize(QSize(760, 220));

  CreateMainWindow();

  m_refresh_timer = new QTimer(this);
  m_refresh_timer->setInterval(250);
  connect(m_refresh_timer, &QTimer::timeout, this, &TriforceCardManagerWindow::RefreshUi);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &TriforceCardManagerWindow::OnEmulationStateChanged);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

TriforceCardManagerWindow::~TriforceCardManagerWindow() = default;

void TriforceCardManagerWindow::RestoreStoredCardsForRunningGame()
{
  RestoreStoredCardsForRunningGameImpl(Core::System::GetInstance());
}

void TriforceCardManagerWindow::CreateMainWindow()
{
  auto* const main_layout = new QVBoxLayout;

  auto* const top_layout = new QHBoxLayout;
  top_layout->addWidget(new QLabel(tr("Game:")));

  m_game = new QComboBox;
  for (CardGameFamily family : {CardGameFamily::VirtuaStriker, CardGameFamily::GekitouProYakyuu,
                                CardGameFamily::KeyOfAvalon})
  {
    m_game->addItem(GetFamilyDisplayName(family), static_cast<int>(family));
  }
  connect(m_game, QOverload<int>::of(&QComboBox::activated), this,
          &TriforceCardManagerWindow::OnGameChanged);
  top_layout->addWidget(m_game, 1);
  main_layout->addLayout(top_layout);

  const QStringList status_texts = {tr("No card selected"), tr("Queued for insert"), tr("Inserted"),
                                    tr("Ejecting"), tr("Pulled")};
  int status_width = 0;
  for (const QString& text : status_texts)
    status_width = std::max(status_width, fontMetrics().horizontalAdvance(text));

  for (std::size_t i = 0; i < SLOT_COUNT; ++i)
  {
    auto* const group = new QGroupBox(tr("Slot %1").arg(i + 1));
    auto* const row_layout = new QHBoxLayout;

    row_layout->addWidget(new QLabel(tr("Card:")));

    m_slots[i].group = group;
    m_slots[i].combo = new QComboBox;
    m_slots[i].combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_slots[i].combo->setMinimumContentsLength(18);
    row_layout->addWidget(m_slots[i].combo, 1);

    m_slots[i].create = new QPushButton(tr("Create"));
    m_slots[i].insert = new QPushButton(tr("Insert"));
    m_slots[i].pull = new QPushButton(tr("Pull"));
    m_slots[i].status = new QLabel;
    m_slots[i].status->setMinimumWidth(status_width + 12);
    m_slots[i].status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    row_layout->addWidget(m_slots[i].create);
    row_layout->addWidget(m_slots[i].insert);
    row_layout->addWidget(m_slots[i].pull);
    row_layout->addWidget(m_slots[i].status);

    group->setLayout(row_layout);
    main_layout->addWidget(group);

    connect(m_slots[i].combo, QOverload<int>::of(&QComboBox::activated), this,
            [this, i] { SetSelectedCard(i); });
    connect(m_slots[i].create, &QPushButton::clicked, this, [this, i] { CreateCard(i); });
    connect(m_slots[i].insert, &QPushButton::clicked, this,
            [this, i] { SetSlotInserted(i, true); });
    connect(m_slots[i].pull, &QPushButton::clicked, this, [this, i] { SetSlotInserted(i, false); });
  }

  auto* const bottom_layout = new QHBoxLayout;
  bottom_layout->addStretch();

  auto* const open_folder = new QPushButton(tr("Open Card Folder"));
  auto* const refresh = new QPushButton(tr("Refresh"));

  connect(open_folder, &QPushButton::clicked, this, &TriforceCardManagerWindow::OpenCardFolder);
  connect(refresh, &QPushButton::clicked, this, &TriforceCardManagerWindow::RefreshUi);

  bottom_layout->addWidget(open_folder);
  bottom_layout->addWidget(refresh);
  main_layout->addLayout(bottom_layout);

  setLayout(main_layout);
}

void TriforceCardManagerWindow::RefreshUi()
{
  const auto state = Core::GetState(Core::System::GetInstance());
  const bool is_running = state == Core::State::Running || state == Core::State::Paused;
  std::optional<CardGameFamily> active_family;
  std::array<SerialInterface::CSIDevice_AMBaseboard::ICCardSlotStatus, SLOT_COUNT> statuses;
  u32 slot_count = GetSlotCount(GetStoredGameFamily());

  if (is_running)
  {
    auto& system = Core::System::GetInstance();
    const Core::CPUThreadGuard guard(system);
    if (auto* const baseboard = GetAMBaseboard(system);
        baseboard && baseboard->GetICCardSlotCount() != 0)
    {
      active_family =
          GetCardGameFamily(static_cast<AMMediaboard::GameType>(AMMediaboard::GetGameType()));
      if (active_family)
      {
        slot_count = baseboard->GetICCardSlotCount();
        for (u32 i = 0; i < slot_count; ++i)
          statuses[i] = baseboard->GetICCardSlotStatus(i);
      }
    }
  }

  const CardGameFamily current_family = active_family.value_or(GetStoredGameFamily());

  {
    QSignalBlocker blocker(m_game);
    m_game->setCurrentIndex(m_game->findData(static_cast<int>(current_family)));
  }
  m_game->setEnabled(!active_family.has_value());
  if (!active_family.has_value())
    slot_count = GetSlotCount(current_family);

  const QString library_path = GetLibraryPath(current_family);

  const QDir library_dir(library_path);
  const QStringList file_names =
      library_dir.entryList({QStringLiteral("*.bin")}, QDir::Files, QDir::Name);

  std::array<std::optional<QString>, SLOT_COUNT> selected_paths;
  for (u32 i = 0; i < SLOT_COUNT; ++i)
  {
    if (!statuses[i].selected_card_path.empty())
      selected_paths[i] = QString::fromStdString(statuses[i].selected_card_path);
    else
      selected_paths[i] = GetStoredSelectedPath(current_family, i);
  }

  for (u32 i = 0; i < SLOT_COUNT; ++i)
  {
    const bool slot_visible = i < slot_count;
    m_slots[i].group->setVisible(slot_visible);
    if (!slot_visible)
      continue;

    QSignalBlocker blocker(m_slots[i].combo);

    m_slots[i].combo->clear();
    m_slots[i].combo->addItem(tr("<No Card Selected>"), QString{});

    for (const QString& file_name : file_names)
    {
      const QString full_path = library_dir.filePath(file_name);
      m_slots[i].combo->addItem(QFileInfo(file_name).completeBaseName(), full_path);
    }

    if (selected_paths[i].has_value() && !selected_paths[i]->isEmpty() &&
        m_slots[i].combo->findData(*selected_paths[i]) == -1)
    {
      const QFileInfo file_info(*selected_paths[i]);
      m_slots[i].combo->addItem(file_info.completeBaseName(), *selected_paths[i]);
    }

    const int current_index = selected_paths[i].has_value() && !selected_paths[i]->isEmpty() ?
                                  m_slots[i].combo->findData(*selected_paths[i]) :
                                  0;
    m_slots[i].combo->setCurrentIndex(current_index >= 0 ? current_index : 0);

    const bool slot_busy =
        statuses[i].wants_inserted || statuses[i].is_present || statuses[i].is_ejecting;
    const bool duplicate_card = slot_count > 1 && selected_paths[i].has_value() &&
                                !selected_paths[i]->isEmpty() &&
                                selected_paths[i] == selected_paths[1 - i];

    auto display_status = statuses[i];
    if (display_status.selected_card_path.empty() && selected_paths[i].has_value())
      display_status.selected_card_path = selected_paths[i]->toStdString();

    m_slots[i].combo->setEnabled(!slot_busy);
    m_slots[i].create->setEnabled(!slot_busy);
    m_slots[i].insert->setEnabled(active_family.has_value() && !slot_busy &&
                                  selected_paths[i].has_value() && !selected_paths[i]->isEmpty() &&
                                  !duplicate_card);
    m_slots[i].pull->setEnabled(active_family.has_value() && slot_busy);
    m_slots[i].status->setText(GetStatusText(display_status));
  }
}

void TriforceCardManagerWindow::OnEmulationStateChanged(Core::State state)
{
  const bool is_running = state == Core::State::Running || state == Core::State::Paused;
  if (is_running)
    m_refresh_timer->start();
  else
    m_refresh_timer->stop();
  RefreshUi();
}

void TriforceCardManagerWindow::OnGameChanged()
{
  SetStoredGameFamily(static_cast<CardGameFamily>(m_game->currentData().toInt()));
  RefreshUi();
}

void TriforceCardManagerWindow::OpenCardFolder()
{
  const QString library_path =
      GetLibraryPath(static_cast<CardGameFamily>(m_game->currentData().toInt()));
  File::CreateDirs(library_path.toStdString());
  QDesktopServices::openUrl(QUrl::fromLocalFile(library_path));
}

void TriforceCardManagerWindow::CreateCard(u32 slot_index)
{
  bool ok = false;
  const QString card_name =
      QInputDialog::getText(this, tr("Create Card"), tr("Card Name:"), QLineEdit::Normal, {}, &ok)
          .trimmed();
  if (!ok || card_name.isEmpty())
    return;

  const CardGameFamily family = static_cast<CardGameFamily>(m_game->currentData().toInt());
  const QString escaped_name =
      QString::fromStdString(Common::EscapeFileName(card_name.toStdString()));
  const QString full_path =
      QDir(GetLibraryPath(family)).filePath(escaped_name + QStringLiteral(".bin"));

  if (QFileInfo::exists(full_path))
  {
    ModalMessageBox::warning(this, tr("Create Card"), tr("A card with that name already exists."));
    return;
  }

  if (!Triforce::ICCardReader::CreateBlankCardFile(full_path.toStdString(),
                                                   GetRepresentativeGameType(family)))
  {
    ModalMessageBox::warning(this, tr("Create Card"), tr("Failed to create the card."));
    return;
  }

  SetStoredSelectedPath(family, slot_index, full_path);

  auto& system = Core::System::GetInstance();
  if (Core::GetState(system) == Core::State::Running ||
      Core::GetState(system) == Core::State::Paused)
  {
    const Core::CPUThreadGuard guard(system);
    if (auto* const baseboard = GetAMBaseboard(system);
        baseboard && baseboard->GetICCardSlotCount() != 0)
      baseboard->SetICCardSlotPath(slot_index, full_path.toStdString());
  }

  RefreshUi();
}

void TriforceCardManagerWindow::SetSelectedCard(u32 slot_index)
{
  const CardGameFamily family = static_cast<CardGameFamily>(m_game->currentData().toInt());
  const QString selected_path = m_slots[slot_index].combo->currentData().toString();
  const auto previous_path = GetStoredSelectedPath(family, slot_index);

  if (!selected_path.isEmpty())
  {
    const auto other_path = GetStoredSelectedPath(family, 1 - slot_index);
    if (other_path.has_value() && selected_path == *other_path)
    {
      ModalMessageBox::warning(this, tr("Select Card"),
                               tr("That card is already selected in the other slot."));
      QSignalBlocker blocker(m_slots[slot_index].combo);
      const int previous_index = previous_path.has_value() && !previous_path->isEmpty() ?
                                     m_slots[slot_index].combo->findData(*previous_path) :
                                     0;
      m_slots[slot_index].combo->setCurrentIndex(previous_index >= 0 ? previous_index : 0);
      return;
    }
  }

  SetStoredSelectedPath(family, slot_index, selected_path);

  auto& system = Core::System::GetInstance();
  if (Core::GetState(system) == Core::State::Running ||
      Core::GetState(system) == Core::State::Paused)
  {
    const Core::CPUThreadGuard guard(system);
    if (auto* const baseboard = GetAMBaseboard(system);
        baseboard && baseboard->GetICCardSlotCount() != 0)
    {
      baseboard->SetICCardSlotPath(slot_index, selected_path.toStdString());
      if (selected_path.isEmpty())
        baseboard->SetICCardSlotInserted(slot_index, false);
    }
  }

  RefreshUi();
}

void TriforceCardManagerWindow::SetSlotInserted(u32 slot_index, bool inserted)
{
  auto& system = Core::System::GetInstance();
  const Core::CPUThreadGuard guard(system);
  if (auto* const baseboard = GetAMBaseboard(system);
      baseboard && baseboard->GetICCardSlotCount() != 0)
    baseboard->SetICCardSlotInserted(slot_index, inserted);

  RefreshUi();
}
