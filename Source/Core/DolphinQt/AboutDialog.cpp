// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/AboutDialog.h"

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtGlobal>

#include "Common/Version.h"

#include "DolphinQt/Resources.h"

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("About Dolphin"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  const QString text =
      QStringLiteral(R"(
<p style='font-size:38pt; font-weight:400;'>Dolphin</p>

<p style='font-size:18pt;'>%VERSION_STRING%</p>

<p style='font-size: small;'>
%BRANCH%<br>
%REVISION%<br><br>
%QT_VERSION%
</p>

<p>
%CHECK_FOR_UPDATES%: <a href='https://dolphin-emu.org/download'>dolphin-emu.org/download</a>
</p>

<p>
%ABOUT_DOLPHIN%
</p>

<p>
%GAMES_YOU_OWN%
</p>

<p>
<a href='https://github.com/dolphin-emu/dolphin/blob/master/COPYING'>%LICENSE%</a> |
<a href='https://github.com/dolphin-emu/dolphin/graphs/contributors'>%AUTHORS%</a> |
<a href='https://forums.dolphin-emu.org/'>%SUPPORT%</a>
)")
          .replace(QStringLiteral("%VERSION_STRING%"),
                   QString::fromUtf8(Common::GetScmDescStr().c_str()))
          .replace(QStringLiteral("%BRANCH%"),
                   // i18n: "Branch" means the version control term, not a literal tree branch.
                   tr("Branch: %1").arg(QString::fromUtf8(Common::GetScmBranchStr().c_str())))
          .replace(QStringLiteral("%REVISION%"),
                   tr("Revision: %1").arg(QString::fromUtf8(Common::GetScmRevGitStr().c_str())))
          .replace(QStringLiteral("%QT_VERSION%"),
                   tr("Using Qt %1").arg(QStringLiteral(QT_VERSION_STR)))
          .replace(QStringLiteral("%CHECK_FOR_UPDATES%"), tr("Check for updates"))
          .replace(QStringLiteral("%ABOUT_DOLPHIN%"),
                   // i18n: The word "free" in the standard phrase "free and open source"
                   // is "free" as in "freedom" - it refers to certain properties of the
                   // software's license, not the software's price. (It is true that Dolphin
                   // can be downloaded at no cost, but that's not what this message says.)
                   tr("Dolphin is a free and open-source GameCube and Wii emulator."))
          .replace(QStringLiteral("%GAMES_YOU_OWN%"),
                   tr("This software should not be used to play games you do not legally own."))
          .replace(QStringLiteral("%LICENSE%"), tr("License"))
          .replace(QStringLiteral("%AUTHORS%"), tr("Authors"))
          .replace(QStringLiteral("%SUPPORT%"), tr("Support"));

  QLabel* text_label = new QLabel(text);
  text_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  text_label->setOpenExternalLinks(true);

  QLabel* copyright = new QLabel(
      QStringLiteral("<small>%1</small>")
          .arg(
              // i18n: This message uses curly quotes in English. If you want to use curly quotes
              // in your translation, please use the type of curly quotes that's appropriate for
              // your language. If you aren't sure which type is appropriate, see
              // https://en.wikipedia.org/wiki/Quotation_mark#Specific_language_features
              tr("\u00A9 2003-2015+ Dolphin Team. \u201cGameCube\u201d and \u201cWii\u201d are "
                 "trademarks of Nintendo. Dolphin is not affiliated with Nintendo in any way.")));

  QLabel* logo = new QLabel();
  logo->setPixmap(Resources::GetAppIcon().pixmap(200, 200));
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
