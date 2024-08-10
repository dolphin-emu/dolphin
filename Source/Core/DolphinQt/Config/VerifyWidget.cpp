// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/VerifyWidget.h"

#include <future>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <QByteArray>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeVerifier.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Settings.h"

VerifyWidget::VerifyWidget(std::shared_ptr<DiscIO::Volume> volume) : m_volume(std::move(volume))
{
  QVBoxLayout* layout = new QVBoxLayout;

  CreateWidgets();
  ConnectWidgets();

  layout->addWidget(m_problems);
  layout->addWidget(m_summary_text);
  layout->addLayout(m_hash_layout);
  layout->addLayout(m_redump_layout);
  layout->addWidget(m_verify_button);

  layout->setStretchFactor(m_problems, 5);
  layout->setStretchFactor(m_summary_text, 2);

  setLayout(layout);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &VerifyWidget::OnEmulationStateChanged);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

void VerifyWidget::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  // Verifying a Wii game while emulation is running doesn't work correctly
  // due to verification of a Wii game creating an instance of IOS
  m_verify_button->setEnabled(!running);
}

void VerifyWidget::CreateWidgets()
{
  m_problems = new QTableWidget(0, 2, this);
  m_problems->setTabKeyNavigation(false);
  m_problems->setHorizontalHeaderLabels({tr("Problem"), tr("Severity")});
  m_problems->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_problems->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_problems->horizontalHeader()->setHighlightSections(false);
  m_problems->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_problems->verticalHeader()->hide();

  m_summary_text = new QTextEdit(this);
  m_summary_text->setReadOnly(true);

  m_hash_layout = new QFormLayout;
  std::tie(m_crc32_checkbox, m_crc32_line_edit) = AddHashLine(m_hash_layout, tr("CRC32:"));
  std::tie(m_md5_checkbox, m_md5_line_edit) = AddHashLine(m_hash_layout, tr("MD5:"));
  std::tie(m_sha1_checkbox, m_sha1_line_edit) = AddHashLine(m_hash_layout, tr("SHA-1:"));

  const auto default_to_calculate = DiscIO::VolumeVerifier::GetDefaultHashesToCalculate();
  m_crc32_checkbox->setChecked(default_to_calculate.crc32);
  m_md5_checkbox->setChecked(default_to_calculate.md5);
  m_sha1_checkbox->setChecked(default_to_calculate.sha1);

  m_redump_layout = new QFormLayout;
  if (DiscIO::IsDisc(m_volume->GetVolumeType()))
  {
    std::tie(m_redump_checkbox, m_redump_line_edit) =
        AddHashLine(m_redump_layout, tr("Redump.org Status:"));
    m_redump_checkbox->setChecked(CanVerifyRedump());
    UpdateRedumpEnabled();
  }
  else
  {
    m_redump_checkbox = nullptr;
    m_redump_line_edit = nullptr;
  }

  // Extend line edits to their maximum possible widths (needed on macOS)
  m_hash_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_redump_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  m_verify_button = new QPushButton(tr("Verify Integrity"), this);
}

std::pair<QCheckBox*, QLineEdit*> VerifyWidget::AddHashLine(QFormLayout* layout, QString text)
{
  QLineEdit* line_edit = new QLineEdit(this);
  line_edit->setReadOnly(true);
  QCheckBox* checkbox = new QCheckBox(tr("Calculate"), this);

  QHBoxLayout* hbox_layout = new QHBoxLayout;
  hbox_layout->addWidget(line_edit);
  hbox_layout->addWidget(checkbox);

  layout->addRow(text, hbox_layout);

  return std::pair(checkbox, line_edit);
}

void VerifyWidget::ConnectWidgets()
{
  connect(m_verify_button, &QPushButton::clicked, this, &VerifyWidget::Verify);

  connect(m_md5_checkbox, &QCheckBox::stateChanged, this, &VerifyWidget::UpdateRedumpEnabled);
  connect(m_sha1_checkbox, &QCheckBox::stateChanged, this, &VerifyWidget::UpdateRedumpEnabled);
}

static void SetHash(QLineEdit* line_edit, const std::vector<u8>& hash)
{
  const QByteArray byte_array = QByteArray::fromRawData(reinterpret_cast<const char*>(hash.data()),
                                                        static_cast<int>(hash.size()));
  line_edit->setText(QString::fromLatin1(byte_array.toHex()));
}

bool VerifyWidget::CanVerifyRedump() const
{
  // We don't allow Redump verification with CRC32 only since generating a collision is too easy
  return m_md5_checkbox->isChecked() || m_sha1_checkbox->isChecked();
}

void VerifyWidget::UpdateRedumpEnabled()
{
  if (m_redump_checkbox)
    m_redump_checkbox->setEnabled(CanVerifyRedump());
}

void VerifyWidget::Verify()
{
  const bool redump_verification =
      CanVerifyRedump() && m_redump_checkbox && m_redump_checkbox->isChecked();

  DiscIO::VolumeVerifier verifier(
      *m_volume, redump_verification,
      {m_crc32_checkbox->isChecked(), m_md5_checkbox->isChecked(), m_sha1_checkbox->isChecked()});

  // We have to divide the number of processed bytes with something so it won't make ints overflow
  constexpr int DIVISOR = 0x100;

  ParallelProgressDialog progress(tr("Verifying"), tr("Cancel"), 0,
                                  static_cast<int>(verifier.GetTotalBytes() / DIVISOR), this);
  progress.GetRaw()->setWindowTitle(tr("Verifying"));
  progress.GetRaw()->setMinimumDuration(500);
  progress.GetRaw()->setWindowModality(Qt::WindowModal);

  auto future =
      std::async(std::launch::async,
                 [&verifier, &progress]() -> std::optional<DiscIO::VolumeVerifier::Result> {
                   progress.SetValue(0);
                   verifier.Start();
                   while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
                   {
                     progress.SetValue(static_cast<int>(verifier.GetBytesProcessed() / DIVISOR));
                     if (progress.WasCanceled())
                       return std::nullopt;

                     verifier.Process();
                   }
                   verifier.Finish();

                   progress.Reset();
                   return verifier.GetResult();
                 });
  SetQWidgetWindowDecorations(progress.GetRaw());
  progress.GetRaw()->exec();

  std::optional<DiscIO::VolumeVerifier::Result> result = future.get();
  if (!result)
    return;

  m_summary_text->setText(QString::fromStdString(result->summary_text));

  m_problems->setRowCount(static_cast<int>(result->problems.size()));
  for (int i = 0; i < m_problems->rowCount(); ++i)
  {
    const DiscIO::VolumeVerifier::Problem problem = result->problems[i];

    QString severity;
    switch (problem.severity)
    {
    case DiscIO::VolumeVerifier::Severity::Low:
      severity = tr("Low");
      break;
    case DiscIO::VolumeVerifier::Severity::Medium:
      severity = tr("Medium");
      break;
    case DiscIO::VolumeVerifier::Severity::High:
      severity = tr("High");
      break;
    case DiscIO::VolumeVerifier::Severity::None:
      break;
    }

    SetProblemCellText(i, 0, QString::fromStdString(problem.text));
    SetProblemCellText(i, 1, severity);
  }

  SetHash(m_crc32_line_edit, result->hashes.crc32);
  SetHash(m_md5_line_edit, result->hashes.md5);
  SetHash(m_sha1_line_edit, result->hashes.sha1);

  if (m_redump_line_edit)
    m_redump_line_edit->setText(QString::fromStdString(result->redump.message));
}

void VerifyWidget::SetProblemCellText(int row, int column, QString text)
{
  QLabel* label = new QLabel(text);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setWordWrap(true);
  label->setMargin(4);
  m_problems->setCellWidget(row, column, label);
}
