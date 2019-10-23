// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Common/Version.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Resources.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("About Dolphin/PrimeHack"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  const QString small =
      QStringLiteral("<p style='margin-top:0; margin-bottom:0; font-size:small;'>");
  const QString medium = QStringLiteral("<p style='margin-top:15px;'>");

  QString text;
  text.append(QStringLiteral("<p style='font-size:38pt; font-weight:400; margin-bottom:0;'>") +
              tr("PrimeHack") + QStringLiteral("</p>"));
  text.append(QStringLiteral("<p style='font-size:18pt; margin-top:0;'>%1</p>")
                  .arg(QString::fromUtf8("v0.3.0 (5.0-10966)")));

  text.append(medium + tr("Check for updates: ") +
              QStringLiteral(
                  "<a href='https://github.com/shiiion/dolphin/releases'>github.com/shiiion/dolphin/releases</a></p>"));
  // i18n: The word "free" in the standard phrase "free and open source"
  // is "free" as in "freedom" - it refers to certain properties of the
  // software's license, not the software's price. (It is true that Dolphin
  // can be downloaded at no cost, but that's not what this message says.)
  text.append(medium + tr("PrimeHack is a mod of Dolphin to implement FPS controls into Metroid Prime Trilogy.") +
              QStringLiteral("</p>"));
  text.append(medium +
              tr("This software should not be used to play games you do not legally own.") +
              QStringLiteral("</p>"));
  text.append(
      medium +
      QStringLiteral(
          "<a href='https://github.com/dolphin-emu/dolphin/blob/master/license.txt'>%1</a> | "
          "<a href='https://discord.gg/invite/hYp5Naz'>%2</a></p>")
          .arg(tr("License"))
          .arg(tr("Discord")));

  QLabel* text_label = new QLabel(text);
  text_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  text_label->setOpenExternalLinks(true);

  QLabel* copyright =
      new QLabel(small +
                 // i18n: This message uses curly quotes in English. If you want to use curly quotes
                 // in your translation, please use the type of curly quotes that's appropriate for
                 // your language. If you aren't sure which type is appropriate, see
                 // https://en.wikipedia.org/wiki/Quotation_mark#Specific_language_features
                 tr("\u00A9 2003-2015+ Dolphin Team. \u201cGameCube\u201d and \u201cWii\u201d are "
                    "trademarks of Nintendo. Dolphin & PrimeHack are not affiliated with Nintendo in any way.") +
                 QStringLiteral("</p>"));

  QLabel* logo = new QLabel();
  logo->setPixmap(Resources::GetMisc(Resources::MiscID::LogoLarge));
  logo->setContentsMargins(30, 0, 30, 0);

  QVBoxLayout* main_layout = new QVBoxLayout;
  QHBoxLayout* h_layout = new QHBoxLayout;

  setLayout(main_layout);
  main_layout->addLayout(h_layout);
  main_layout->addWidget(copyright);
  copyright->setAlignment(Qt::AlignCenter);
  copyright->setContentsMargins(0, 15, 0, 0);

  h_layout->setAlignment(Qt::AlignLeft);
  h_layout->addWidget(logo);
  h_layout->addWidget(text_label);
}
