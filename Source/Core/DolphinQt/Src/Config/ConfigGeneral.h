#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class DControlStateManager;

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

	DControlStateManager* ctrlManager;
};
