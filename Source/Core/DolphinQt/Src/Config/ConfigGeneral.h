#include <QTabWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class DControlStateManager;

class DConfigMainGeneralTab : public QTabWidget
{
	Q_OBJECT

public:
    DConfigMainGeneralTab(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();

private:
	QWidget* CreateCoreTabWidget(QWidget* parent);
	QWidget* CreatePathsTabWidget(QWidget* parent);

	QCheckBox* cbDualCore;
	QCheckBox* cbIdleSkipping;
	QCheckBox* cbCheats;

	QComboBox* chFramelimit;
	QButtonGroup* rbCPUEngine;

	QCheckBox* cbConfirmOnStop;
	QCheckBox* cbRenderToMain;

	DControlStateManager* ctrlManager;
};
