// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QClipboard>
#include <QPushButton>
#include <QThread>

#include "ui_SystemInfo.h"

#include "Common/Common.h"
#include "Common/CPUDetect.h"

#include "DolphinQt/SystemInfo.h"

DSystemInfo::DSystemInfo(QWidget* parent_widget) :
	QDialog(parent_widget)
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);

	m_ui = std::make_unique<Ui::DSystemInfo>();
	m_ui->setupUi(this);

	UpdateSystemInfo();

	QPushButton* btn = m_ui->buttonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
	connect(btn, &QPushButton::pressed, this, &DSystemInfo::CopyPressed);
}

DSystemInfo::~DSystemInfo()
{
}

void DSystemInfo::CopyPressed()
{
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setText(m_ui->txtSysInfo->toPlainText());
}

void DSystemInfo::UpdateSystemInfo()
{
	QString sysinfo;

	sysinfo += QStringLiteral("System\n===========================\n");
	sysinfo += QStringLiteral("OS: %1\n").arg(GetOS());
	sysinfo += QStringLiteral("CPU: %1, %2 logical processors\n")
		.arg(QString::fromStdString(cpu_info.Summarize()))
		.arg(QThread::idealThreadCount());

	sysinfo += QStringLiteral("\nDolphin\n===========================\n");
	sysinfo += QStringLiteral("SCM: branch %1, rev %2\n")
		.arg(QString::fromUtf8(scm_branch_str))
		.arg(QString::fromUtf8(scm_rev_git_str));

	sysinfo += QStringLiteral("\nGUI\n===========================\n");
	sysinfo += QStringLiteral("Compiled for Qt: %1\n").arg(QStringLiteral(QT_VERSION_STR));
	sysinfo += QStringLiteral("Running on Qt: %1").arg(QString::fromUtf8(qVersion()));

	m_ui->txtSysInfo->setPlainText(sysinfo);
}

QString DSystemInfo::GetOS() const
{
	QString ret;
	/* DON'T REORDER WITHOUT READING Qt DOCS! */
#if defined(Q_OS_WIN)
	ret += QStringLiteral("Windows ");
	switch (QSysInfo::WindowsVersion) {
	case QSysInfo::WV_VISTA: ret += QStringLiteral("Vista"); break;
	case QSysInfo::WV_WINDOWS7: ret += QStringLiteral("7"); break;
	case QSysInfo::WV_WINDOWS8: ret += QStringLiteral("8"); break;
	case QSysInfo::WV_WINDOWS8_1: ret += QStringLiteral("8.1"); break;
	case QSysInfo::WV_WINDOWS10: ret += QStringLiteral("10"); break;
	default: ret += QStringLiteral("(unknown)"); break;
	}
#elif defined(Q_OS_MAC)
	ret += QStringLiteral("Mac OS X ");
	switch (QSysInfo::MacintoshVersion) {
	case QSysInfo::MV_10_9: ret += QStringLiteral("10.9"); break;
	case QSysInfo::MV_10_10: ret += QStringLiteral("10.10"); break;
	case QSysInfo::MV_10_11: ret += QStringLiteral("10.11"); break;
	default: ret += QStringLiteral("(unknown)"); break;
	}
#elif defined(Q_OS_LINUX)
	ret += QStringLiteral("Linux");
#elif defined(Q_OS_FREEBSD)
	ret += QStringLiteral("FreeBSD");
#elif defined(Q_OS_OPENBSD)
	ret += QStringLiteral("OpenBSD");
#elif defined(Q_OS_NETBSD)
	ret += QStringLiteral("NetBSD");
#elif defined(Q_OS_BSD4)
	ret += QStringLiteral("Other BSD");
#elif defined(Q_OS_UNIX)
	ret += QStringLiteral("Unix");
#else
	ret += QStringLiteral("Unknown");
#endif

#if defined(Q_WS_X11) || defined(Q_OS_X11)
	ret += QStringLiteral(" X11");
#elif defined(Q_WS_WAYLAND)
	ret += QStringLiteral(" Wayland");
#endif

	return ret;
}
