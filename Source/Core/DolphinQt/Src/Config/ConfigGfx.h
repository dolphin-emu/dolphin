#include <QTabWidget>

class QCheckBox;
class QComboBox;
class DControlStateManager;
class DConfigGfx : public QTabWidget
{
	Q_OBJECT

	enum EfbCopyMethod
	{
		ECM_DISABLED,
		ECM_FAST,
		ECM_FASTPRETTY,
		ECM_ACCURATE,
	};

public:
	DConfigGfx(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();

private:
	QWidget* CreateGeneralTabWidget();
	QWidget* CreateEnhancementsTabWidget();
	QWidget* CreateEmulationTabWidget();
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

	// Emulation
	QComboBox* chEfbCopyMethod;

	DControlStateManager* ctrlManager;
};
