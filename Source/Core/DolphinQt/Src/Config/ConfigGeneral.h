#include <QTabWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QListWidget;
class DControlStateManager;

class DConfigMainGeneralTab : public QTabWidget
{
	Q_OBJECT

public:
    DConfigMainGeneralTab(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();

	void OnAddIsoPath();
	void OnRemoveIsoPath();
	void OnClearIsoPathList();

private:
	QWidget* CreateCoreTabWidget(QWidget* parent);
	QWidget* CreatePathsTabWidget(QWidget* parent);

	// Core
	QCheckBox* cbDualCore;
	QCheckBox* cbIdleSkipping;
	QCheckBox* cbCheats;

	QComboBox* chFramelimit;
	QButtonGroup* rbCPUEngine;

	QCheckBox* cbConfirmOnStop;
	QCheckBox* cbRenderToMain;

	// Paths
	QListWidget* pathList;

	DControlStateManager* ctrlManager;
};
