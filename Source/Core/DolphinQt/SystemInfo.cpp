// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QClipboard>
#include <QPushButton>
#include <QThread>

#include "ui_SystemInfo.h"

#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Common/StdMakeUnique.h"

#include "DolphinQt/SystemInfo.h"
#include "DolphinQt/Utils/Utils.h"

DSystemInfo::DSystemInfo(QWidget* parent_widget) :
	QDialog(parent_widget)
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);

	m_ui = std::make_unique<Ui::DSystemInfo>();
	m_ui->setupUi(this);

	UpdateSystemInfo();

	QPushButton* btn = m_ui->buttonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
	connect(btn, SIGNAL(pressed()), this, SLOT(btnCopy_pressed()));
}

DSystemInfo::~DSystemInfo()
{
}

void DSystemInfo::btnCopy_pressed()
{
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setText(m_ui->txtSysInfo->toPlainText());
}

void DSystemInfo::UpdateSystemInfo()
{
	QString sysinfo;

	sysinfo += SL("System\n===========================\n");
	sysinfo += SL("OS: %1\n").arg(GetOS());
	sysinfo += SL("CPU: %1, %2 cores\n").arg(QString::fromStdString(cpu_info.Summarize()))
		.arg(QThread::idealThreadCount());

	sysinfo += SL("\nDolphin\n===========================\n");
	sysinfo += SL("SCM: branch %1, rev %2\n").arg(SC(scm_branch_str)).arg(SC(scm_rev_git_str));
	sysinfo += SL("Compiled: %1, %2\n").arg(SL(__DATE__)).arg(SL(__TIME__));

	sysinfo += SL("\nGUI\n===========================\n");
	sysinfo += SL("Compiled for Qt: %1\n").arg(SL(QT_VERSION_STR));
	sysinfo += SL("Running on Qt: %1").arg(SC(qVersion()));

	m_ui->txtSysInfo->setPlainText(sysinfo);
}

QString DSystemInfo::GetOS()
{
	QString ret;
	/* DON'T REORDER WITHOUT READING Qt DOCS! */
#if defined(Q_OS_WIN)
	ret += SL("Windows ");
	switch (QSysInfo::WindowsVersion) {
	case QSysInfo::WV_VISTA: ret += SL("Vista"); break;
	case QSysInfo::WV_WINDOWS7: ret += SL("7"); break;
	case QSysInfo::WV_WINDOWS8: ret += SL("8"); break;
	default: ret += SL("(unknown)"); break;
	}
#elif defined(Q_OS_MAC)
	ret += SL("Mac OS X ");
	switch (QSysInfo::MacintoshVersion) {
	case QSysInfo::MV_10_7: ret += SL("10.7"); break;
	case QSysInfo::MV_10_8: ret += SL("10.8"); break;
	case QSysInfo::MV_10_9: ret += SL("10.9"); break;
	default: ret += SL("(unknown)"); break;
	}
#elif defined(Q_OS_LINUX)
	ret += SL("Linux");
#elif defined(Q_OS_FREEBSD)
	ret += SL("FreeBSD");
#elif defined(Q_OS_OPENBSD)
	ret += SL("OpenBSD");
#elif defined(Q_OS_NETBSD)
	ret += SL("NetBSD");
#elif defined(Q_OS_BSD4)
	ret += SL("Other BSD");
#elif defined(Q_OS_UNIX)
	ret += SL("Unix");
#else
	ret += SL("Unknown");
#endif

#if defined(Q_WS_X11) || defined(Q_OS_X11)
	ret += SL(" X11");
#elif defined(Q_WS_WAYLAND)
	ret += SL(" Wayland");
#endif

	return ret;
}
