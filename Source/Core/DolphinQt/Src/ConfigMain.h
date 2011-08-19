#pragma once

#include <QTabWidget>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QButtonGroup;
class QPushButton;
class QSlider;
class QDiag;
class QListWidgetItem;
class QStackedWidget;
class QListWidget;

class DConfigMainGeneralTab : public QWidget
{
	Q_OBJECT

public:
    DConfigMainGeneralTab(QWidget* parent = NULL);
    virtual ~DConfigMainGeneralTab();

public slots:
	void Reset();
	void Apply();

private:
	QCheckBox* cbDualCore;
	QCheckBox* cbIdleSkipping;
	QCheckBox* cbCheats;

	QComboBox* chFramelimit;
	QButtonGroup* rbCPUEngine;

	QCheckBox* cbConfirmOnStop;
	QCheckBox* cbRenderToMain;
};

class DConfigMainSoundTab : public QWidget
{
	Q_OBJECT

public:
	DConfigMainSoundTab(QWidget* parent = NULL);
	virtual ~DConfigMainSoundTab();

public slots:
	void Reset();
	void Apply();

private:
	QButtonGroup* rbDSPEngine;
	QComboBox* chBackend;
	QComboBox* chSamplingRate;
	QCheckBox* cbEnableDTK;
	QCheckBox* cbRecordAudio;
	QCheckBox* cbLLEOnThread;
	QDiag* dgVolume;
};

class DConfigDialog : public QDialog
{
	Q_OBJECT

public:
	enum InitialConfigItem
	{
		ICI_General = 0,
		ICI_Graphics = 1,
		ICI_Sound = 2,
		ICI_GCPad = 3,
		ICI_Wiimote = 4,
	};

	DConfigDialog(InitialConfigItem initialConfigItem, QWidget* parent = NULL);
    virtual ~DConfigDialog();

public slots:
	void switchPage(QListWidgetItem*,QListWidgetItem*);
	void OnApply();
	void OnOk();

private:
	void closeEvent(QCloseEvent*);

	QStackedWidget* stackWidget;
	QListWidget* menusView;

signals:
	void Apply();
};
