#include "ConfigManager.h"

#include "RenderWindow.h"

DRenderWindow::DRenderWindow(QWidget* parent): QWidget(parent)
{

}

DRenderWindow::~DRenderWindow()
{

}

void DRenderWindow::closeEvent(QCloseEvent* ev)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos = pos().x();
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos = pos().y();
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = width(); // TODO: Make sure these are client sizes!
	SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = height();

	// TODO: Do this differently...
	emit Closed();
    QWidget::closeEvent(ev);
}
