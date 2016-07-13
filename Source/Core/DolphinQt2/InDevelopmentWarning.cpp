// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QCommandLinkButton>
#include <QFileInfo>
#include <QLabel>
#include <QLinearGradient>
#include <QMessageBox>
#include <QProcess>

#include "DolphinQt2/InDevelopmentWarning.h"
#include "DolphinQt2/Resources.h"

static QBrush MakeConstructionBrush()
{
  QLinearGradient gradient;
  gradient.setSpread(QGradient::RepeatSpread);
  gradient.setStart(0, 16);
  gradient.setFinalStop(16, 0);
  gradient.setColorAt(0, Qt::yellow);
  gradient.setColorAt(0.25, Qt::yellow);
  gradient.setColorAt(0.251, Qt::black);
  gradient.setColorAt(0.75, Qt::black);
  gradient.setColorAt(0.751, Qt::yellow);
  gradient.setColorAt(1.0, Qt::yellow);

  return QBrush(gradient);
}

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
  QWidget* strip_container = new QWidget(this);
  QLabel* strip = new QLabel(tr("DolphinQt Experimental GUI"), strip_container);
  QLabel* body = new QLabel(this);

  QCommandLinkButton* btn_dolphinwx =
      new QCommandLinkButton(tr("Run DolphinWX Instead"), tr("Recommended for normal users"), this);
  QCommandLinkButton* btn_run = new QCommandLinkButton(tr("Use DolphinQt Anyway"), this);
  QCommandLinkButton* btn_close = new QCommandLinkButton(tr("Exit Dolphin"), this);

  QPalette yellow_tape_palette{palette()};
  yellow_tape_palette.setBrush(QPalette::Window, MakeConstructionBrush());
  yellow_tape_palette.setColor(QPalette::WindowText, Qt::black);

  QPalette yellow_text_palette{palette()};
  yellow_text_palette.setColor(QPalette::Window, Qt::yellow);
  yellow_text_palette.setColor(QPalette::WindowText, Qt::black);

  QFont heading_font{strip->font()};
  heading_font.setPointSizeF(heading_font.pointSizeF() * 2);
  heading_font.setBold(true);
  strip->setFont(heading_font);
  strip->setPalette(yellow_text_palette);
  strip->setAutoFillBackground(true);
  strip->setContentsMargins(15, 5, 15, 5);
  strip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  strip_container->setPalette(yellow_tape_palette);
  strip_container->setAutoFillBackground(true);
  strip_container->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  QHBoxLayout* strip_layout = new QHBoxLayout(strip_container);
  strip_layout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
  strip_layout->addWidget(strip);
  strip_layout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
  strip_layout->setMargin(10);
  strip_layout->setSpacing(0);

  QString body_text = tr(
      "<p>DolphinQt is a new experimental GUI that is eventually intended to replace "
      "the current GUI based on wxWidgets. The implementation is <b>very "
      "incomplete</b>, even basic functionality like changing settings and "
      "attaching gamepads may be missing or not work at all.</p>\n"
      "<p>Only developers working on the DolphinQt implementation should use it at "
      "the present time; normal users are recommended to continue using the "
      "older DolphinWX GUI for the time being.</p>\n"
      "<p><big><b>Bug Reports:</b></big> At the current time there is no point making bug reports "
      "about the DolphinQt GUI as the list of what is broken is much longer "
      "than the list of things that work.</p>\n");
  body->setText(body_text);
  body->setWordWrap(true);
  body->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  connect(btn_dolphinwx, &QCommandLinkButton::clicked, [this](bool) {
    if (!LaunchDolphinWX())
      QMessageBox::critical(
          this, tr("Failed to launch"),
          tr("Could not start DolphinWX. Check for dolphin.exe in the installation directory."));
    reject();
  });
  connect(btn_run, &QCommandLinkButton::clicked, this, [this](bool) { accept(); });
  connect(btn_close, &QCommandLinkButton::clicked, this, [this](bool) { reject(); });

  QVBoxLayout* button_column = new QVBoxLayout();
  button_column->addWidget(btn_dolphinwx);
  button_column->addWidget(btn_run);
  button_column->addWidget(btn_close);
  button_column->setMargin(0);
  button_column->setSpacing(10);

  QHBoxLayout* button_padding = new QHBoxLayout();
  button_padding->addSpacerItem(new QSpacerItem(10, 20, QSizePolicy::Expanding));
  button_padding->addLayout(button_column);
  button_padding->addSpacerItem(new QSpacerItem(10, 20, QSizePolicy::Expanding));
  button_padding->setStretch(0, 1);
  button_padding->setStretch(1, 2);
  button_padding->setStretch(2, 1);
  button_padding->setMargin(0);
  button_padding->setSpacing(0);

  QVBoxLayout* body_column = new QVBoxLayout();
  body_column->addWidget(body);
  body_column->addSpacerItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Expanding));
  body_column->addLayout(button_padding);
  body_column->addSpacerItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Expanding));
  body_column->setMargin(10);

  QVBoxLayout* top_level_layout = new QVBoxLayout(this);
  top_level_layout->addWidget(strip_container);
  top_level_layout->addLayout(body_column);
  top_level_layout->setSpacing(0);
  top_level_layout->setMargin(0);

  setWindowIcon(Resources::GetMisc(Resources::LOGO_SMALL));
  setWindowTitle(tr("DolphinQt2 Experimental GUI"));
  setMinimumSize(480, 340);
}

InDevelopmentWarning::~InDevelopmentWarning()
{
}
