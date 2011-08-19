#include <QPixmap>

#include "Volume.h"

class Resources
{
public:
	static void Init();

	static QPixmap& GetDolphinIcon();
	static QPixmap& GetPlatformPixmap(int console);
	static QPixmap& GetRegionPixmap(DiscIO::IVolume::ECountry region);
	static QPixmap& GetRatingPixmap(int rating);
	static QPixmap& GetToolbarPixmap(int id);

	enum
	{
		TOOLBAR_OPEN = 0,
		TOOLBAR_REFRESH,
		TOOLBAR_BROWSE,
		TOOLBAR_PLAY,
		TOOLBAR_STOP,
		TOOLBAR_FULLSCREEN,
		TOOLBAR_SCREENSHOT,
		TOOLBAR_CONFIGURE,
		TOOLBAR_PLUGIN_GFX,
		TOOLBAR_PLUGIN_DSP,
		TOOLBAR_PLUGIN_GCPAD,
		TOOLBAR_PLUGIN_WIIMOTE,
		TOOLBAR_HELP,
		TOOLBAR_PAUSE,
		NUM_TOOLBAR_ICONS
	};

private:
	Resources();
	~Resources();

	static Resources* instance;
	QVector<QPixmap> platforms;
	QVector<QPixmap> regions;
	QVector<QPixmap> ratings;
	QVector<QPixmap> toolbar;
	QPixmap dolphin;
};
