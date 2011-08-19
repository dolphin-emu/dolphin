#include <QtGui>
#include "Resources.h"
#include "../resources/Platform_Gamecube.c"
#include "../resources/Platform_Wad.c"
#include "../resources/Platform_Wii.c"
#include "../resources/Flag_Europe.c"
#include "../resources/Flag_France.c"
#include "../resources/Flag_Italy.c"
#include "../resources/Flag_Japan.c"
#include "../resources/Flag_Taiwan.c"
#include "../resources/Flag_Unknown.c"
#include "../resources/Flag_USA.c"
#include "../resources/rating0.c"
#include "../resources/rating1.c"
#include "../resources/rating2.c"
#include "../resources/rating3.c"
#include "../resources/rating4.c"
#include "../resources/rating5.c"
#include "../resources/Dolphin.c"
#include "../resources/toolbar_file_open.c"
#include "../resources/toolbar_browse.c"
#include "../resources/toolbar_fullscreen.c"
#include "../resources/toolbar_help.c"
#include "../resources/toolbar_pause.c"
#include "../resources/toolbar_play.c"
#include "../resources/toolbar_plugin_dsp.c"
#include "../resources/toolbar_plugin_gfx.c"
#include "../resources/toolbar_plugin_options.c"
#include "../resources/toolbar_plugin_pad.c"
#include "../resources/toolbar_plugin_wiimote.c"
#include "../resources/toolbar_refresh.c"
#include "../resources/toolbar_stop.c"
#include "../resources/no_banner.cpp"

#include "Volume.h"

Resources* Resources::instance = NULL;

Resources::Resources()
{

}

Resources::~Resources()
{

}

void Resources::Init()
{
	instance = new Resources;

	instance->regions.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	instance->regions[DiscIO::IVolume::COUNTRY_EUROPE].loadFromData(flag_europe_png, sizeof(flag_europe_png));
	instance->regions[DiscIO::IVolume::COUNTRY_FRANCE].loadFromData(flag_france_png, sizeof(flag_france_png));
	instance->regions[DiscIO::IVolume::COUNTRY_RUSSIA].loadFromData(flag_unknown_png, sizeof(flag_unknown_png)); // TODO
	instance->regions[DiscIO::IVolume::COUNTRY_USA].loadFromData(flag_usa_png, sizeof(flag_usa_png));
	instance->regions[DiscIO::IVolume::COUNTRY_JAPAN].loadFromData(flag_japan_png, sizeof(flag_japan_png));
	instance->regions[DiscIO::IVolume::COUNTRY_KOREA].loadFromData(flag_unknown_png, sizeof(flag_unknown_png)); // TODO
	instance->regions[DiscIO::IVolume::COUNTRY_ITALY].loadFromData(flag_italy_png, sizeof(flag_italy_png));
	instance->regions[DiscIO::IVolume::COUNTRY_TAIWAN].loadFromData(flag_taiwan_png, sizeof(flag_taiwan_png));
	instance->regions[DiscIO::IVolume::COUNTRY_SDK].loadFromData(flag_unknown_png, sizeof(flag_unknown_png)); // TODO
	instance->regions[DiscIO::IVolume::COUNTRY_UNKNOWN].loadFromData(flag_unknown_png, sizeof(flag_unknown_png));

	instance->platforms.resize(3);
	instance->platforms[0].loadFromData(platform_gamecube_png, sizeof(platform_gamecube_png));
	instance->platforms[1].loadFromData(platform_wii_png, sizeof(platform_wii_png));
	instance->platforms[2].loadFromData(platform_wad_png, sizeof(platform_wad_png));

	instance->ratings.resize(6);
	instance->ratings[0].loadFromData(rating0_png, sizeof(rating0_png));
	instance->ratings[1].loadFromData(rating1_png, sizeof(rating1_png));
	instance->ratings[2].loadFromData(rating2_png, sizeof(rating2_png));
	instance->ratings[3].loadFromData(rating3_png, sizeof(rating3_png));
	instance->ratings[4].loadFromData(rating4_png, sizeof(rating4_png));
	instance->ratings[5].loadFromData(rating5_png, sizeof(rating5_png));

	instance->pixmaps.resize(NUM_ICONS);
	instance->pixmaps[TOOLBAR_OPEN].loadFromData(toolbar_file_open_png, sizeof(toolbar_file_open_png));
	instance->pixmaps[TOOLBAR_REFRESH].loadFromData(toolbar_refresh_png, sizeof(toolbar_refresh_png));
	instance->pixmaps[TOOLBAR_BROWSE].loadFromData(toolbar_browse_png, sizeof(toolbar_browse_png));
	instance->pixmaps[TOOLBAR_PLAY].loadFromData(toolbar_play_png, sizeof(toolbar_play_png));
	instance->pixmaps[TOOLBAR_STOP].loadFromData(toolbar_stop_png, sizeof(toolbar_stop_png));
	instance->pixmaps[TOOLBAR_FULLSCREEN].loadFromData(toolbar_fullscreen_png, sizeof(toolbar_fullscreen_png));
	instance->pixmaps[TOOLBAR_SCREENSHOT].loadFromData(toolbar_fullscreen_png, sizeof(toolbar_fullscreen_png));
	instance->pixmaps[TOOLBAR_CONFIGURE].loadFromData(toolbar_plugin_options_png, sizeof(toolbar_plugin_options_png));
	instance->pixmaps[TOOLBAR_PLUGIN_GFX].loadFromData(toolbar_plugin_gfx_png, sizeof(toolbar_plugin_gfx_png));
	instance->pixmaps[TOOLBAR_PLUGIN_DSP].loadFromData(toolbar_plugin_dsp_png, sizeof(toolbar_plugin_dsp_png));
	instance->pixmaps[TOOLBAR_PLUGIN_GCPAD].loadFromData(toolbar_plugin_pad_png, sizeof(toolbar_plugin_pad_png));
	instance->pixmaps[TOOLBAR_PLUGIN_WIIMOTE].loadFromData(toolbar_plugin_wiimote_png, sizeof(toolbar_plugin_wiimote_png));
	instance->pixmaps[TOOLBAR_HELP].loadFromData(toolbar_help_png, sizeof(toolbar_help_png));
	instance->pixmaps[TOOLBAR_PAUSE].loadFromData(toolbar_pause_png, sizeof(toolbar_pause_png));
	// TODO: instance->toolbar[MEMCARD];
	// TODO: instance->toolbar[HOTKEYS];
	instance->pixmaps[DOLPHIN_LOGO].loadFromData(dolphin_ico32x32, sizeof(dolphin_ico32x32));
	instance->pixmaps[BANNER_MISSING].loadFromData(no_banner_png, sizeof(no_banner_png));
}

QPixmap& Resources::GetRegionPixmap(DiscIO::IVolume::ECountry region)
{
	return instance->regions[region];
}

QPixmap& Resources::GetPlatformPixmap(int console)
{
	if (console >= instance->platforms.size()) return instance->platforms[0];
	return instance->platforms[console];
}

QPixmap& Resources::GetRatingPixmap(int rating)
{
	if (rating >= instance->ratings.size()) return instance->ratings[0];
	return instance->ratings[rating];
}

QPixmap& Resources::GetPixmap(int id)
{
	if (id >= instance->pixmaps.size()) return instance->pixmaps[0];
	return instance->pixmaps[id];
}

QIcon Resources::GetIcon(int id)
{
	return QIcon(GetPixmap(id));
}
