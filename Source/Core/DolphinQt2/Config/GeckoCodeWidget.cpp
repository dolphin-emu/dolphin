// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/GeckoCodeWidget.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinQt2/Config/CheatWarningWidget.h"
#include "DolphinQt2/GameList/GameFile.h"

GeckoCodeWidget::GeckoCodeWidget(const GameFile& game)
    : m_game(game), m_game_id(game.GetGameID().toStdString()), m_game_revision(game.GetRevision())
{
  CreateWidgets();
  ConnectWidgets();

  IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  IniFile game_ini_default = SConfig::GetInstance().LoadDefaultGameIni(m_game_id, m_game_revision);
  m_gecko_codes = Gecko::LoadCodes(game_ini_default, game_ini_local);

  UpdateList();
}

void GeckoCodeWidget::CreateWidgets()
{
  m_warning = new CheatWarningWidget(m_game_id);
  m_code_list = new QListWidget;
  m_name_label = new QLabel;
  m_creator_label = new QLabel;

  QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

  const auto line_height = QFontMetrics(font()).lineSpacing();

  m_code_description = new QTextEdit;
  m_code_description->setFont(monospace);
  m_code_description->setReadOnly(true);
  m_code_description->setFixedHeight(line_height * 5);

  m_code_view = new QTextEdit;
  m_code_view->setFont(monospace);
  m_code_view->setReadOnly(true);
  m_code_view->setFixedHeight(line_height * 10);

  m_download_codes = new QPushButton(tr("Download Codes (WiiRD Database)"));
  m_download_codes->setEnabled(!m_game_id.empty());
  m_download_codes->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_warning);
  layout->addWidget(m_code_list);

  auto* info_layout = new QFormLayout;

  info_layout->addRow(tr("Name:"), m_name_label);
  info_layout->addRow(tr("Creator:"), m_creator_label);
  info_layout->addRow(tr("Description:"), static_cast<QWidget*>(nullptr));

  info_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

  for (QLabel* label : {m_name_label, m_creator_label})
  {
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setCursor(Qt::IBeamCursor);
  }

  layout->addLayout(info_layout);
  layout->addWidget(m_code_description);
  layout->addWidget(m_code_view);
  layout->addWidget(m_download_codes, 0, Qt::AlignRight);

  setLayout(layout);
}

void GeckoCodeWidget::ConnectWidgets()
{
  connect(m_code_list, &QListWidget::itemSelectionChanged, this,
          &GeckoCodeWidget::OnSelectionChanged);
  connect(m_code_list, &QListWidget::itemChanged, this, &GeckoCodeWidget::OnItemChanged);

  connect(m_download_codes, &QPushButton::pressed, this, &GeckoCodeWidget::DownloadCodes);

  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this,
          &GeckoCodeWidget::OpenGeneralSettings);
}

void GeckoCodeWidget::OnSelectionChanged()
{
  auto items = m_code_list->selectedItems();

  if (items.empty())
    return;

  auto selected = items[0];

  const auto& code = m_gecko_codes[m_code_list->row(selected)];

  m_name_label->setText(QString::fromStdString(code.name));
  m_creator_label->setText(QString::fromStdString(code.creator));

  m_code_description->clear();

  for (const auto& line : code.notes)
    m_code_description->append(QString::fromStdString(line));

  m_code_view->clear();

  for (const auto& c : code.codes)
    m_code_view->append(QStringLiteral("%1  %2")
                            .arg(c.address, 8, 16, QLatin1Char('0'))
                            .arg(c.data, 8, 16, QLatin1Char('0')));
}

void GeckoCodeWidget::OnItemChanged(QListWidgetItem* item)
{
  m_gecko_codes[m_code_list->row(item)].enabled = (item->checkState() == Qt::Checked);

  SaveCodes();
}

void GeckoCodeWidget::SaveCodes()
{
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
  Gecko::SaveCodes(game_ini_local, m_gecko_codes);

  game_ini_local.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
}

void GeckoCodeWidget::UpdateList()
{
  m_code_list->clear();

  for (size_t i = 0; i < m_gecko_codes.size(); i++)
  {
    const auto& code = m_gecko_codes[i];

    auto* item = new QListWidgetItem(QString::fromStdString(code.name)
                                         .replace(QStringLiteral("&lt;"), QStringLiteral("<"))
                                         .replace(QStringLiteral("&gt;"), QStringLiteral(">")));

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(code.enabled ? Qt::Checked : Qt::Unchecked);

    m_code_list->addItem(item);
  }
}

void GeckoCodeWidget::DownloadCodes()
{
  bool success;

  std::vector<Gecko::GeckoCode> codes = Gecko::DownloadCodes(m_game_id, &success);

  if (!success)
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to download codes."));
    return;
  }

  if (codes.empty())
  {
    QMessageBox::critical(this, tr("Error"), tr("File contained no codes."));
    return;
  }

  size_t added_count = 0;

  for (const auto& code : codes)
  {
    auto it = std::find(m_gecko_codes.begin(), m_gecko_codes.end(), code);

    if (it == m_gecko_codes.end())
    {
      m_gecko_codes.push_back(code);
      added_count++;
    }
  }

  UpdateList();
  SaveCodes();

  QMessageBox::information(this, tr("Download complete"),
                           tr("Downloaded %1 codes. (added %2)")
                               .arg(QString::number(codes.size()), QString::number(added_count)));
}
