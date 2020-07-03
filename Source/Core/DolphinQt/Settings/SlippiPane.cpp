#include "DolphinQt/Settings/SlippiPane.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

SlippiPane::SlippiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
}

void SlippiPane::BrowseReplayFolder()
{
  QString dir = QDir::toNativeSeparators(
      QFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath()));
  if (!dir.isEmpty())
  {
    m_replay_folder_edit->setText(dir);
    // XXX set replay folder
  }
}

void SlippiPane::CreateLayout()
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* layout = new QVBoxLayout();
  setLayout(layout);

  auto* replay_settings = new QGroupBox(tr("Replay Settings"));
  auto* replay_settings_layout = new QVBoxLayout();
  replay_settings->setLayout(replay_settings_layout);
  layout->addWidget(replay_settings);

  m_enable_replay_save_checkbox = new QCheckBox(tr("Save Slippi Replays"));
  m_enable_replay_save_checkbox->setToolTip(tr(
      "Enable this to make Slippi automatically save .slp recordings of your games."));
  replay_settings_layout->addWidget(m_enable_replay_save_checkbox);

  m_enable_monthly_replay_folders_checkbox = new QCheckBox(tr("Save Replays to Monthly Subfolders"));
  m_enable_monthly_replay_folders_checkbox->setToolTip(tr(
      "Enable this to save your replays into subfolders by month (YYYY-MM)."));
  replay_settings_layout->addWidget(m_enable_monthly_replay_folders_checkbox);

  auto *replay_folder_layout = new QGridLayout();
  m_replay_folder_edit = new QLineEdit(); // XXX fill in default string
  m_replay_folder_edit->setToolTip(tr("Choose where your Slippi replay files are saved."));
  QPushButton* replay_folder_open = new QPushButton(QStringLiteral("..."));
  connect(replay_folder_open, &QPushButton::clicked, this, &SlippiPane::BrowseReplayFolder);
  replay_folder_layout->addWidget(new QLabel(tr("Replay Folder:")), 0, 0);
  replay_folder_layout->addWidget(m_replay_folder_edit, 0, 1);
  replay_folder_layout->addWidget(replay_folder_open, 0, 2);
  replay_settings_layout->addLayout(replay_folder_layout);

  auto* online_settings = new QGroupBox(tr("Online Settings"));
  auto* online_settings_layout = new QFormLayout();
  online_settings->setLayout(online_settings_layout);
  layout->addWidget(online_settings);

  m_delay_spin = new QSpinBox();
  m_delay_spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_delay_spin->setRange(1, 9);
  m_delay_spin->setToolTip(tr(
    "Leave this at 2 unless consistently playing on 120+ ping. "
    "Increasing this can cause unplayable input delay, and lowering it can cause visual artifacts/lag."));
  online_settings_layout->addRow(tr("Delay Frames:"), m_delay_spin);
}
