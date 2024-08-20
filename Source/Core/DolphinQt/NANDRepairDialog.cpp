// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NANDRepairDialog.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <fmt/format.h>

#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"
#include "DiscIO/WiiSaveBanner.h"
#include "DolphinQt/Resources.h"

NANDRepairDialog::NANDRepairDialog(const WiiUtils::NANDCheckResult& result, QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(tr("NAND Check"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowIcon(Resources::GetAppIcon());

  QVBoxLayout* main_layout = new QVBoxLayout();

  QLabel* damaged_label =
      new QLabel(tr("The emulated NAND is damaged. System titles such as the Wii Menu and "
                    "the Wii Shop Channel may not work correctly."));
  damaged_label->setWordWrap(true);
  main_layout->addWidget(damaged_label);

  if (!result.titles_to_remove.empty())
  {
    QLabel* warning_label =
        new QLabel(tr("WARNING: Fixing this NAND requires the deletion of titles that have "
                      "incomplete data on the NAND, including all associated save data. "
                      "By continuing, the following title(s) will be removed:"));
    warning_label->setWordWrap(true);
    main_layout->addWidget(warning_label);

    std::string title_listings;
    const DiscIO::Language language = SConfig::GetInstance().GetCurrentLanguage(true);
    for (const u64 title_id : result.titles_to_remove)
    {
      Core::TitleDatabase title_db;
      title_listings += fmt::format("{:016x}", title_id);

      const std::string database_name = title_db.GetChannelName(title_id, language);
      if (!database_name.empty())
      {
        title_listings += " - " + database_name;
      }
      else
      {
        DiscIO::WiiSaveBanner banner(title_id);
        if (banner.IsValid())
        {
          title_listings += " - " + banner.GetName();
          const std::string description = banner.GetDescription();
          if (!StripWhitespace(description).empty())
            title_listings += " - " + description;
        }
      }

      title_listings += "\n";
    }

    QPlainTextEdit* title_box = new QPlainTextEdit(QString::fromStdString(title_listings));
    title_box->setReadOnly(true);
    main_layout->addWidget(title_box);

    QLabel* maybe_fix_label = new QLabel(tr("Launching these titles may also fix the issues."));
    maybe_fix_label->setWordWrap(true);
    main_layout->addWidget(maybe_fix_label);
  }

  QLabel* question_label = new QLabel(tr("Do you want to try to repair the NAND?"));
  question_label->setWordWrap(true);
  main_layout->addWidget(question_label);

  QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
  main_layout->addWidget(button_box);

  QHBoxLayout* top_layout = new QHBoxLayout();

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
  QLabel* icon_label = new QLabel;
  icon_label->setPixmap(icon.pixmap(100));
  icon_label->setAlignment(Qt::AlignTop);
  top_layout->addWidget(icon_label);
  top_layout->addSpacing(10);

  top_layout->addLayout(main_layout);

  setLayout(top_layout);
  resize(600, 400);

  connect(button_box->button(QDialogButtonBox::Yes), &QPushButton::clicked, this, &QDialog::accept);
  connect(button_box->button(QDialogButtonBox::No), &QPushButton::clicked, this, &QDialog::reject);
}
