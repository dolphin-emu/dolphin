#include "ConfigGfx.h"

DConfigGfx::DConfigGfx(QWidget* parent) : QTabWidget(parent)
{
	addTab(new QWidget, tr("General"));
	addTab(new QWidget, tr("Enhancements"));
	addTab(new QWidget, tr("Hacks"));
	addTab(new QWidget, tr("Advanced"));
}

void DConfigGfx::Reset()
{

}

void DConfigGfx::Apply()
{
	// TODO: Children can be found via findChild()!
	
	// TODO: Instead: QDialog::accept?
}
