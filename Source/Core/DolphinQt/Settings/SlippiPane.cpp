#include "DolphinQt/Settings/SlippiPane.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"

SlippiPane::SlippiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
}

void SlippiPane::BrowseReplayFolder()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
      this, tr("Select Replay Folder"),
      QString::fromStdString(SConfig::GetInstance().m_strSlippiReplayDir)));
  if (!dir.isEmpty())
  {
    m_replay_folder_edit->setText(dir);
    SConfig::GetInstance().m_strSlippiReplayDir = dir.toStdString();
  }
}

void SlippiPane::CreateLayout()
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* layout = new QVBoxLayout();
  setLayout(layout);

#ifndef IS_PLAYBACK
  // Replay Settings
  auto* replay_settings = new QGroupBox(tr("Replay Settings"));
  auto* replay_settings_layout = new QVBoxLayout();
  replay_settings->setLayout(replay_settings_layout);
  layout->addWidget(replay_settings);

  auto* enable_replay_save_checkbox = new QCheckBox(tr("Save Slippi Replays"));
  enable_replay_save_checkbox->setToolTip(
      tr("Enable this to make Slippi automatically save .slp recordings of your games."));
  replay_settings_layout->addWidget(enable_replay_save_checkbox);
  enable_replay_save_checkbox->setChecked(SConfig::GetInstance().m_slippiSaveReplays);
  connect(enable_replay_save_checkbox, &QCheckBox::toggled, this,
          [](bool checked) { SConfig::GetInstance().m_slippiSaveReplays = checked; });

  auto* enable_monthly_replay_folders_checkbox =
      new QCheckBox(tr("Save Replays to Monthly Subfolders"));
  enable_monthly_replay_folders_checkbox->setToolTip(
      tr("Enable this to save your replays into subfolders by month (YYYY-MM)."));
  replay_settings_layout->addWidget(enable_monthly_replay_folders_checkbox);
  enable_monthly_replay_folders_checkbox->setChecked(
      SConfig::GetInstance().m_slippiReplayMonthFolders);
  connect(enable_monthly_replay_folders_checkbox, &QCheckBox::toggled, this,
          [](bool checked) { SConfig::GetInstance().m_slippiReplayMonthFolders = checked; });

  auto* replay_folder_layout = new QGridLayout();
  m_replay_folder_edit =
      new QLineEdit(QString::fromStdString(SConfig::GetInstance().m_strSlippiReplayDir));
  m_replay_folder_edit->setToolTip(tr("Choose where your Slippi replay files are saved."));
  connect(m_replay_folder_edit, &QLineEdit::editingFinished, [this] {
    SConfig::GetInstance().m_strSlippiReplayDir = m_replay_folder_edit->text().toStdString();
  });
  QPushButton* replay_folder_open = new QPushButton(QStringLiteral("..."));
  connect(replay_folder_open, &QPushButton::clicked, this, &SlippiPane::BrowseReplayFolder);
  replay_folder_layout->addWidget(new QLabel(tr("Replay Folder:")), 0, 0);
  replay_folder_layout->addWidget(m_replay_folder_edit, 0, 1);
  replay_folder_layout->addWidget(replay_folder_open, 0, 2);
  replay_settings_layout->addLayout(replay_folder_layout);

  // Online Settings
  auto* online_settings = new QGroupBox(tr("Online Settings"));
  auto* online_settings_layout = new QFormLayout();
  online_settings->setLayout(online_settings_layout);
  layout->addWidget(online_settings);

  auto* delay_spin = new QSpinBox();
  delay_spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  delay_spin->setRange(1, 9);
  delay_spin->setToolTip(tr("Leave this at 2 unless consistently playing on 120+ ping. "
                            "Increasing this can cause unplayable input delay, and lowering it "
                            "can cause visual artifacts/lag."));
  online_settings_layout->addRow(tr("Delay Frames:"), delay_spin);
  delay_spin->setValue(SConfig::GetInstance().m_slippiOnlineDelay);
  connect(delay_spin, qOverload<int>(&QSpinBox::valueChanged), this,
          [](int delay) { SConfig::GetInstance().m_slippiOnlineDelay = delay; });

#endif

#ifdef IS_PLAYBACK
  //Playback Settings
  auto* playback_settings = new QGroupBox(tr("Playback Settings"));
  auto* playback_settings_layout = new QVBoxLayout();
  playback_settings->setLayout(playback_settings_layout);
  layout->addWidget(playback_settings);

  auto* enable_playback_seek_checkbox = new QCheckBox(tr("Enable Seekbar"));
  char seekbarTooltip[] = "<html><head/><body><p>Enables video player style controls while watching Slippi replays. Uses more cpu resources and can be stuttery. " \
    "Space: Pause/Play " \
    "Left/Right: Jump 5 seconds back/forward" \
    "Shift + Left/Right: Jump 20 seconds back/forward" \
    "Period (while paused): Advance one frame";
  enable_playback_seek_checkbox->setToolTip(tr(seekbarTooltip));
  playback_settings_layout->addWidget(enable_playback_seek_checkbox);
  enable_playback_seek_checkbox->setChecked(SConfig::GetInstance().m_slippiEnableSeek);
  connect(enable_playback_seek_checkbox, &QCheckBox::toggled, this,
    [](bool checked) { SConfig::GetInstance().m_slippiEnableSeek = checked; });
}
#endif
