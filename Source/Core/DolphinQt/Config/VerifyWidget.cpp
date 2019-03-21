// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/VerifyWidget.h"

#include <memory>

#include <QHeaderView>
#include <QLabel>
#include <QProgressDialog>
#include <QVBoxLayout>

#include "DiscIO/Volume.h"
#include "DiscIO/VolumeVerifier.h"

VerifyWidget::VerifyWidget(std::shared_ptr<DiscIO::Volume> volume) : m_volume(std::move(volume))
{
  QVBoxLayout* layout = new QVBoxLayout(this);

  CreateWidgets();
  ConnectWidgets();

  layout->addWidget(m_problems);
  layout->addWidget(m_summary_text);
  layout->addWidget(m_verify_button);

  layout->setStretchFactor(m_problems, 5);
  layout->setStretchFactor(m_summary_text, 2);

  setLayout(layout);
}

void VerifyWidget::CreateWidgets()
{
  m_problems = new QTableWidget(0, 2, this);
  m_problems->setHorizontalHeaderLabels({tr("Problem"), tr("Severity")});
  m_problems->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_problems->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_problems->horizontalHeader()->setHighlightSections(false);
  m_problems->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_problems->verticalHeader()->hide();

  m_summary_text = new QTextEdit(this);
  m_summary_text->setReadOnly(true);

  m_verify_button = new QPushButton(tr("Verify Integrity"), this);
}

void VerifyWidget::ConnectWidgets()
{
  connect(m_verify_button, &QPushButton::clicked, this, &VerifyWidget::Verify);
}

void VerifyWidget::Verify()
{
  DiscIO::VolumeVerifier verifier(*m_volume);

  // We have to divide the number of processed bytes with something so it won't make ints overflow
  constexpr int DIVISOR = 0x100;

  QProgressDialog* progress = new QProgressDialog(tr("Verifying"), tr("Cancel"), 0,
                                                  verifier.GetTotalBytes() / DIVISOR, this);
  progress->setWindowTitle(tr("Verifying"));
  progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  progress->setMinimumDuration(500);
  progress->setWindowModality(Qt::WindowModal);

  verifier.Start();
  while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
  {
    progress->setValue(verifier.GetBytesProcessed() / DIVISOR);
    if (progress->wasCanceled())
      return;

    verifier.Process();
  }
  verifier.Finish();

  DiscIO::VolumeVerifier::Result result = verifier.GetResult();
  progress->setValue(verifier.GetBytesProcessed() / DIVISOR);

  m_summary_text->setText(QString::fromStdString(result.summary_text));

  m_problems->setRowCount(static_cast<int>(result.problems.size()));
  for (int i = 0; i < m_problems->rowCount(); ++i)
  {
    const DiscIO::VolumeVerifier::Problem problem = result.problems[i];

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
    }

    SetProblemCellText(i, 0, QString::fromStdString(problem.text));
    SetProblemCellText(i, 1, severity);
  }
}

void VerifyWidget::SetProblemCellText(int row, int column, QString text)
{
  QLabel* label = new QLabel(text);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setWordWrap(true);
  label->setMargin(4);
  m_problems->setCellWidget(row, column, label);
}
