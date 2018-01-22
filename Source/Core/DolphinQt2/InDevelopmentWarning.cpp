// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QBoxLayout>
#include <QCommandLinkButton>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QStyle>

#include "DolphinQt2/InDevelopmentWarning.h"
#include "DolphinQt2/Resources.h"

static bool LaunchDolphinWX()
{
  QFileInfo file_info;
  for (auto path : {"/Dolphin.exe", "/dolphin-emu", "/Dolphin"})
  {
    file_info.setFile(qApp->applicationDirPath() + QString::fromUtf8(path));
    if (file_info.isExecutable())
      return QProcess::startDetached(file_info.absoluteFilePath(), {});
  }
  return false;
}

InDevelopmentWarning::InDevelopmentWarning(QWidget* parent)
    : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
  QWidget* container = new QWidget(this);
  QDialogButtonBox* std_buttons =
      new QDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, this);
  QLabel* heading = new QLabel(container);
  QLabel* icon = new QLabel(container);
  QLabel* body = new QLabel(container);

  QCommandLinkButton* btn_dolphinwx = new QCommandLinkButton(
      tr("Run DolphinWX Instead"), tr("Recommended for normal users"), container);
  QCommandLinkButton* btn_run = new QCommandLinkButton(tr("Use DolphinQt Anyway"), container);

  container->setForegroundRole(QPalette::Text);
  container->setBackgroundRole(QPalette::Base);
  container->setAutoFillBackground(true);

  std_buttons->setContentsMargins(10, 0, 10, 0);

  QFont heading_font{heading->font()};
  heading_font.setPointSizeF(heading_font.pointSizeF() * 1.5);
  heading->setFont(heading_font);
  heading->setText(tr("DolphinQt Experimental GUI"));
  heading->setForegroundRole(QPalette::Text);
  heading->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  heading->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  icon->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxWarning, nullptr, this));
  icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QString body_text = tr(
      "<p>DolphinQt is a new experimental GUI that is intended to replace "
      "the current GUI based on wxWidgets. The implementation is <b>currently "
      "incomplete</b> so some functionality (like changing certain settings) may be missing.</p>\n"
      "<p>Only developers working on the DolphinQt implementation and curious testers should use "
      "it at "
      "the present time; normal users are recommended to continue using the "
      "older DolphinWX GUI for the time being.</p>\n"
      "<h3>Bug Reports</h3><p>At the current time there is no point making bug reports "
      "about DolphinQt GUI's missing features as the developers are already aware of those.</p>\n");
  body->setText(body_text);
  body->setWordWrap(true);
  body->setForegroundRole(QPalette::Text);
  body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  body->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  body->setMinimumWidth(QFontMetrics(body->font()).averageCharWidth() * 76);

  btn_dolphinwx->setDefault(true);

  connect(btn_dolphinwx, &QCommandLinkButton::clicked, this, [this](bool) {
    if (!LaunchDolphinWX())
      QMessageBox::critical(
          this, tr("Failed to launch"),
          tr("Could not start DolphinWX. Check for dolphin.exe in the installation directory."));
    reject();
  });
  connect(btn_run, &QCommandLinkButton::clicked, this, [this](bool) { accept(); });
  connect(std_buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this,
          [this](bool) { reject(); });

  QVBoxLayout* body_column = new QVBoxLayout();
  body_column->addWidget(heading);
  body_column->addWidget(body);
  body_column->addWidget(btn_dolphinwx);
  body_column->addWidget(btn_run);
  body_column->setMargin(0);
  body_column->setSpacing(10);

  QHBoxLayout* icon_layout = new QHBoxLayout(container);
  icon_layout->addWidget(icon, 0, Qt::AlignTop);
  icon_layout->addLayout(body_column);
  icon_layout->setContentsMargins(15, 10, 10, 10);
  icon_layout->setSpacing(15);

  QVBoxLayout* top_layout = new QVBoxLayout(this);
  top_layout->addWidget(container);
  top_layout->addWidget(std_buttons);
  top_layout->setSpacing(10);
  top_layout->setContentsMargins(0, 0, 0, 10);
  top_layout->setSizeConstraint(QLayout::SetMinimumSize);

  setWindowIcon(Resources::GetMisc(Resources::LOGO_SMALL));
  setWindowTitle(tr("DolphinQt2 Experimental GUI"));
}

InDevelopmentWarning::~InDevelopmentWarning()
{
}
