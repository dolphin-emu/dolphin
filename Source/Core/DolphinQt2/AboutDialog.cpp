// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

#include "AboutDialog.h"
#include "Resources.h"
#include "Common/Common.h"

AboutDialog::AboutDialog(QWidget* parent)
		: QDialog(parent)
{
	setWindowTitle(tr("About Dolphin"));
	setAttribute(Qt::WA_DeleteOnClose);

	QString text = QStringLiteral("");
	QString small = QStringLiteral("<p style='margin-top:0px; margin-bottom:0px; font-size:9pt;'>");
	QString medium = QStringLiteral("<p style='margin-top:15px; font-size:11pt;'>");

	text.append(QStringLiteral("<p style='font-size:50pt; font-weight:bold; margin-bottom:0px;'>") +
		tr("Dolphin") + QStringLiteral("</p>"));
	text.append(QStringLiteral("<p style='font-size:18pt; margin-top:0px;'>%1</p>")
		.arg(QString::fromUtf8(scm_desc_str)));

    text.append(small + tr("Branch: ") + QString::fromUtf8(scm_branch_str) + QStringLiteral("</p>"));
	text.append(small + tr("Revision: ") + QString::fromUtf8(scm_rev_git_str) + QStringLiteral("</p>"));
	text.append(small + tr("Compiled: ") + QStringLiteral(__DATE__ " " __TIME__ "</p>"));
	text.append(small + tr("Interface: ") + QStringLiteral("Qt</p>"));

	text.append(medium + tr("Check for updates: ") +
		QStringLiteral("<a href='https://dolphin-emu.org/download'>dolphin-emu.org/download</a></p>"));
	text.append(medium + tr("Dolphin is a free and open-source GameCube and Wii emulator.") + QStringLiteral("</p>"));
	text.append(medium + tr("This software should not be used to play games you do not legally own.") + QStringLiteral("</p>"));
	text.append(medium + QStringLiteral(
		"<a href='https://github.com/dolphin-emu/dolphin/blob/master/license.txt'>%1</a> | "
		"<a href='https://github.com/dolphin-emu/dolphin/graphs/contributors'>%2</a> | "
		"<a href='https://forums.dolphin-emu.org/'>%3</a></p>"//TODO: mebe HR
    ).arg(tr("Licence")).arg(tr("Authors")).arg(tr("Support")));

	QLabel* text_label = new QLabel(text);
	text_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	text_label->setOpenExternalLinks(true);

	QLabel* copyright = new QLabel(tr(
		"© 2003-%1 Dolphin Team. “GameCube” and “Wii” are"
		" trademarks of Nintendo. Dolphin is not affiliated with Nintendo in any way."
	).arg(QStringLiteral(__DATE__).right(4)));

	QLabel* logo = new QLabel();
	logo->setPixmap(Resources::GetMisc(Resources::LOGO_LARGE));
	logo->setContentsMargins(30, 0, 30, 0);

	QVBoxLayout* layout = new QVBoxLayout;
	QHBoxLayout* h_layout = new QHBoxLayout;

	setLayout(layout);
	layout->addLayout(h_layout);
	layout->addWidget(copyright);
	copyright->setAlignment(Qt::AlignCenter);
	copyright->setContentsMargins(0, 15, 0, 0);

	h_layout->setAlignment(Qt::AlignLeft);
	h_layout->addWidget(logo);
	h_layout->addWidget(text_label);
}