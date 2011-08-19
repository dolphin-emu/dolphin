#include "ConfigGfx.h"

DConfigGfx::DConfigGfx(QWidget* parent) : QTabWidget(parent)
{
	addTab(new QWidget, tr("General"));
	addTab(new QWidget, tr("Enhancements"));
	addTab(new QWidget, tr("Hacks"));
	addTab(new QWidget, tr("Advanced"));
}

DConfigGfx::~DConfigGfx()
{

}

void DConfigGfx::Reset()
{

}

void DConfigGfx::Apply()
{

}
