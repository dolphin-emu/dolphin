#include <QTabWidget>

class QCheckBox;
class QComboBox;
class DControlStateManager;
class DConfigGfx : public QTabWidget
{
	Q_OBJECT

public:
	DConfigGfx(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();

private:
	QWidget* CreateGeneralTabWidget();
	QWidget* CreateEnhancementsTabWidget();
	QWidget* CreateHacksTabWidget();
	QWidget* CreateAdvancedTabWidget();

	// General
	QComboBox* cbBackend;
	QComboBox* chAspectRatio;
	QCheckBox* cbVsync;
	QCheckBox* cbFullscreen;

	QCheckBox* cbFPS;
	QCheckBox* cbAutoWindowSize;
	QCheckBox* cbHideCursor;
	QCheckBox* cbRenderToMain;

	// Enhancements
	QComboBox* chInternalResolution;
	QComboBox* chAntiAliasing;
	QComboBox* chAnisotropicFiltering;
	QCheckBox* cbPerPixelLighting;

	DControlStateManager* ctrlManager;
};
