#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QMap>
#include <QVector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDiag;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QRadioButton;
class QSlider;
class QStackedWidget;

// keeps track of initial button state and resets it on request.
// also can notify other widgets when a control's value was changed
class DControlStateManager : public QObject
{
	Q_OBJECT

public:
	DConfigControlManager(QObject* parent);

	void RegisterControl(QCheckBox* control, bool checked);
	void RegisterControl(QRadioButton* control, bool checked);
	void RegisterControl(QComboBox* control, int choice);
	void RegisterControl(QComboBox* control, const QString& choice);

public slots:
	void OnReset();

private:
	QMap<QCheckBox*, bool> checkbox_states;
	QMap<QRadioButton*, bool> radiobutton_states;
	QMap<QComboBox*, int> combobox_states_int;
	QMap<QComboBox*, QString> combobox_states_string;

signals:
	void settingChanged(); // TODO!
};

class DConfigMainGeneralTab : public QWidget
{
	Q_OBJECT

public:
    DConfigMainGeneralTab(QWidget* parent = NULL);

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

	DConfigControlManager* ctrlManager;
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

public slots:
	void switchPage(QListWidgetItem*,QListWidgetItem*);
	void OnReset();
	void OnApply();
	void OnOk();

private:
	void closeEvent(QCloseEvent*);

	QStackedWidget* stackWidget;
	QListWidget* menusView;

signals:
	void Apply();
	void Reset();
};
