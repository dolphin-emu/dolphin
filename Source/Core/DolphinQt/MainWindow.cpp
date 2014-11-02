// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

#include "ui_MainWindow.h"

#include "Common/StdMakeUnique.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/SystemInfo.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

// The "g_main_window" object as defined in MainWindow.h
DMainWindow* g_main_window = nullptr;

DMainWindow::DMainWindow(QWidget* parent_widget)
	: QMainWindow(parent_widget)
{
	m_ui = std::make_unique<Ui::DMainWindow>();
	m_ui->setupUi(this);
#ifdef Q_OS_MACX
	m_ui->toolbar->setMovable(false);
#endif

	Resources::Init();

	UpdateIcons();
	setWindowIcon(Resources::GetIcon(Resources::DOLPHIN_LOGO));

	connect(this, SIGNAL(CoreStateChanged(Core::EState)), this, SLOT(OnCoreStateChanged(Core::EState)));
	emit CoreStateChanged(Core::CORE_UNINITIALIZED); // update GUI items
}

DMainWindow::~DMainWindow()
{
}

// Emulation

void DMainWindow::StartGame(const QString filename)
{
	m_render_widget = std::make_unique<DRenderWidget>();
	m_render_widget->setWindowTitle(tr("Dolphin")); // TODO
	m_render_widget->setWindowIcon(windowIcon());

	// TODO: When rendering to main, this won't resize the parent window..
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		connect(m_render_widget.get(), SIGNAL(Closed()), this, SLOT(on_actStop_triggered()));
		m_render_widget->move(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos);
		m_render_widget->resize(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth, // TODO: Make sure these are client sizes!
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight);
		m_render_widget->show();
	}
	else
	{
		m_ui->centralWidget->addWidget(m_render_widget.get());
		m_ui->centralWidget->setCurrentWidget(m_render_widget.get());
	}

	if (!BootManager::BootCore(filename.toStdString()))
	{
		QMessageBox::critical(this, tr("Fatal error"), tr("Failed to init Core"), QMessageBox::Ok);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_ui->centralWidget->removeWidget(m_render_widget.get());
		else
			m_render_widget->close();
		m_render_widget.reset();
	}
	else
	{
		// TODO: Disable screensaver!

		// TODO: Fullscreen
		//DoFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

		m_render_widget->focusWidget();
		emit CoreStateChanged(Core::CORE_RUN);
	}
}

QString DMainWindow::RequestBootFilename()
{
	// If a game is already selected, just return the filename
	//  ... TODO

	return ShowFileDialog();
}

QString DMainWindow::ShowFileDialog()
{
	return QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
		tr("All supported ROMs (%1);;All files (*)")
		.arg(SL("*.gcm *.iso *.ciso *.gcz *.wbfs *.elf *.dol *.dff *.tmd *.wad")));
}

void DMainWindow::DoStartPause()
{
	if (Core::GetState() == Core::CORE_RUN)
	{
		Core::SetState(Core::CORE_PAUSE);
		emit CoreStateChanged(Core::CORE_PAUSE);
	}
	else
	{
		Core::SetState(Core::CORE_RUN);
		emit CoreStateChanged(Core::CORE_RUN);
	}
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
		m_render_widget->setCursor(Qt::BlankCursor);
}

void DMainWindow::on_actOpen_triggered()
{
	StartGame(ShowFileDialog());
}

void DMainWindow::on_actPlay_triggered()
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		DoStartPause();
	}
	else
	{
		// initialize Core and boot the game
		QString filename = RequestBootFilename();
		if (!filename.isNull())
			StartGame(filename);
	}
}

void DMainWindow::on_actStop_triggered()
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED && !m_isStopping)
	{
		m_isStopping = true;
		// Ask for confirmation in case the user accidentally clicked Stop / Escape
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop)
		{
			int ret = QMessageBox::question(m_render_widget.get(), tr("Please confirm..."),
				tr("Do you want to stop the current emulation?"),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

			if (ret == QMessageBox::No)
				return;
		}

		// TODO: Movie stuff
		// TODO: Show the author/description dialog here

		// TODO: Show busy cursor
		BootManager::Stop();
		// TODO: Hide busy cursor again

		// TODO: Allow screensaver again
		// TODO: Restore original window title

		// TODO: Return from fullscreen if necessary (DoFullscreen in the wx code)

		// TODO:
		// If batch mode was specified on the command-line, exit now.
		//if (m_bBatchMode)
		//	Close(true);

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_ui->centralWidget->removeWidget(m_render_widget.get());
		else
			m_render_widget->close();
		m_render_widget.reset();

		emit CoreStateChanged(Core::CORE_UNINITIALIZED);
	}
	m_isStopping = false;
}

void DMainWindow::OnCoreStateChanged(Core::EState state)
{
	bool is_not_initialized = (state == Core::CORE_UNINITIALIZED);
	bool is_running = (state == Core::CORE_RUN);
	bool is_paused = (state == Core::CORE_PAUSE);

	// Update the toolbar
	m_ui->actPlay->setEnabled(is_not_initialized || is_running || is_paused);
	if (is_running)
	{
		m_ui->actPlay->setIcon(Resources::GetIcon(Resources::TOOLBAR_PAUSE));
		m_ui->actPlay->setText(tr("Pause"));
	}
	else if (is_paused || is_not_initialized)
	{
		m_ui->actPlay->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLAY));
		m_ui->actPlay->setText(tr("Play"));
	}

	m_ui->actStop->setEnabled(!is_not_initialized);
	m_ui->actOpen->setEnabled(is_not_initialized);
}

// DRenderWidget
void DMainWindow::RenderWidgetSize(int& x_pos, int& y_pos, int& w, int& h)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		x_pos = x();
		y_pos = y();
	}
	else
	{
		x_pos = m_render_widget->x();
		y_pos = m_render_widget->y();
	}
	w = m_render_widget->width();
	h = m_render_widget->height();
}

bool DMainWindow::RenderWidgetHasFocus()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
		return isActiveWindow();
	else if (m_render_widget != nullptr)
		return m_render_widget->isActiveWindow();
	else
		return false;
}

// Update all the icons used in DMainWindow with fresh ones from
// "Resources". Call this function after changing the icon theme.
void DMainWindow::UpdateIcons()
{
	m_ui->actOpen->setIcon(Resources::GetIcon(Resources::TOOLBAR_OPEN));
	m_ui->actStop->setIcon(Resources::GetIcon(Resources::TOOLBAR_STOP));
}

// Help menu
void DMainWindow::on_actWebsite_triggered()
{
    QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/")));
}

void DMainWindow::on_actOnlineDocs_triggered()
{
	QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/docs/guides/")));
}

void DMainWindow::on_actGitHub_triggered()
{
	QDesktopServices::openUrl(QUrl(SL("https://github.com/dolphin-emu/dolphin/")));
}

void DMainWindow::on_actSystemInfo_triggered()
{
	DSystemInfo* dlg = new DSystemInfo(this);
	dlg->open();
}

void DMainWindow::on_actAbout_triggered()
{
	DAboutDialog* dlg = new DAboutDialog(this);
	dlg->open();
}
