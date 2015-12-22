// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <clocale>
#include <QString>
#include <string>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Translator.h"

#ifdef _WIN32
#include <windows.h>
#include "gettext-0.19.4/libgnuintl.h"
#endif

void Translator::Init()
{
#ifdef _WIN32
	std::string domain = File::GetSysDirectory() + "../Languages";
#elif defined(__LINUX__)// why doesnt this work
	std::string domain = File::GetSysDirectory() + "../../locale";
#elif defined(__APPLE__)
	std::string domain = File::GetBundleDirectory() + "Contents/Resources";
#else
	std::string domain = File::GetSysDirectory() + "../../locale";
#endif
	bindtextdomain("dolphin-emu", domain.c_str());
	bind_textdomain_codeset("dolphin-emu", "utf-8");
	textdomain("dolphin-emu");
	setlocale(LC_ALL, "");

	Language language_id = static_cast<Language>(SConfig::GetInstance().m_InterfaceLanguage);
	if (language_id != Language::DEFAULT)
		SetLanguage(language_id);

	RegisterStringTranslator([] (const char* string) -> std::string {return gettext(string);});
}

void Translator::SetLanguage(Language language_id)
{
#ifdef _WIN32
	SetThreadLocale(GetLCID(language_id));
#endif
	setlocale(LC_ALL, GetLocale(language_id));
}

QString Translator::translate(const char* context, const char* source_text, const char*  disambiguation, int n) const
{
	return QString::fromUtf8(gettext(source_text));
}

const char* Translator::GetLocale(Language language_id)
{
	switch (language_id)
	{
	case Language::ARABIC: return "ar.UTF-8";
	case Language::CATALAN: return "ca_ES.UTF-8";
	case Language::CHINESE_SIMPLIFIED: return "zh_CN.UTF-8";
	case Language::CHINESE_TRADITIONAL: return "zh_TW.UTF-8";
	case Language::CZECH: return "cs_CZ.UTF-8";
	case Language::DUTCH: return "nl_NL.UTF-8";
	case Language::ENGLISH: return "en_GB.UTF-8";
	case Language::FARSI: return "fa_IR.UTF-8";
	case Language::FRENCH: return "fr_FR.UTF-8";
	case Language::GERMAN: return "de_DE.UTF-8";
	case Language::GREEK: return "el_GR.UTF-8";
	case Language::HEBREW: return "he_IL.UTF-8";
	case Language::HUNGARIAN: return "hu_HU.UTF-8";
	case Language::ITALIAN: return "it_IT.UTF-8";
	case Language::JAPANESE: return "ja_JP.UTF-8";
	case Language::KOREAN: return "ko_KR.UTF-8";
	case Language::NORWEGIAN_BOKMAL: return "nb_NO.UTF-8";
	case Language::POLISH: return "pl_PL.UTF-8";
	case Language::PORTUGUESE: return "pt_PT.UTF-8";
	case Language::PORTUGUESE_BRAZILIAN: return "pt_BR.UTF-8";
	case Language::RUSSIAN: return "ru_RU.UTF-8";
	case Language::SERBIAN: return "sr_RS.UTF-8";
	case Language::SPANISH: return "es_ES.UTF-8";
	case Language::SWEDISH: return "sv_SE.UTF-8";
	case Language::TURKISH: return "tr_TR.UTF-8";
	default: return ""; //setlocale() interprets the empty string as the system locale
	}
}

#ifdef _WIN32
LCID Translator::GetLCID(Language language_id)
{
	switch (language_id)
	{
	case Language::ARABIC: return LANG_ARABIC;
	case Language::CATALAN: return LANG_CATALAN;
	case Language::CHINESE_SIMPLIFIED: return LANG_CHINESE_SIMPLIFIED;
	case Language::CHINESE_TRADITIONAL: return LANG_CHINESE_TRADITIONAL;
	case Language::CZECH: return LANG_CZECH;
	case Language::DUTCH: return LANG_DUTCH;
	case Language::ENGLISH: return LANG_ENGLISH;
	case Language::FARSI: return LANG_FARSI;
	case Language::FRENCH: return LANG_FRENCH;
	case Language::GERMAN: return LANG_GERMAN;
	case Language::GREEK: return LANG_GREEK;
	case Language::HEBREW: return LANG_HEBREW;
	case Language::HUNGARIAN: return LANG_HUNGARIAN;
	case Language::ITALIAN: return LANG_ITALIAN;
	case Language::JAPANESE: return LANG_JAPANESE;
	case Language::KOREAN: return LANG_KOREAN;
	case Language::NORWEGIAN_BOKMAL: return SUBLANG_NORWEGIAN_BOKMAL;
	case Language::POLISH: return LANG_POLISH;
	case Language::PORTUGUESE: return LANG_PORTUGUESE;
	case Language::PORTUGUESE_BRAZILIAN: return SUBLANG_PORTUGUESE_BRAZILIAN;
	case Language::RUSSIAN: return LANG_RUSSIAN;
	case Language::SERBIAN: return LANG_SERBIAN;
	case Language::SPANISH: return LANG_SPANISH;
	case Language::SWEDISH: return LANG_SWEDISH;
	case Language::TURKISH: return LANG_TURKISH;
	default: return LANG_NEUTRAL;
	}
}
#endif

//map enum to native language name
const QString Translator::DisplayLanguage(Language language_id)
{
	switch (language_id)
	{
	case Language::ARABIC: return QString::fromUtf8(u8"\u0627\u0644\u0639\u0631\u0628\u064A\u0629");
	case Language::CATALAN: return QString::fromUtf8(u8"Catal\u00E0");
	case Language::CHINESE_SIMPLIFIED: return QString::fromUtf8(u8"\u7B80\u4F53\u4E2D\u6587");
	case Language::CHINESE_TRADITIONAL: return QString::fromUtf8(u8"\u7E41\u9AD4\u4E2D\u6587");
	case Language::CZECH: return QString::fromUtf8(u8"\u010Ce\u0161tina");
	case Language::DUTCH: return QString::fromUtf8(u8"Nederlands");
	case Language::ENGLISH: return QString::fromUtf8(u8"English");
	case Language::FARSI: return QString::fromUtf8(u8"\u0641\u0627\u0631\u0633\u06CC");
	case Language::FRENCH: return QString::fromUtf8(u8"Fran\u00E7ais");
	case Language::GERMAN: return QString::fromUtf8(u8"Deutsch");
	case Language::GREEK: return QString::fromUtf8(u8"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC");
	case Language::HEBREW: return QString::fromUtf8(u8"\u05E2\u05D1\u05E8\u05D9\u05EA");
	case Language::HUNGARIAN: return QString::fromUtf8(u8"Magyar");
	case Language::ITALIAN: return QString::fromUtf8(u8"Italiano");
	case Language::JAPANESE: return QString::fromUtf8(u8"\u65E5\u672C\u8A9E");
	case Language::KOREAN: return QString::fromUtf8(u8"\uD55C\uAD6D\uC5B4");
	case Language::NORWEGIAN_BOKMAL: return QString::fromUtf8(u8"Norsk bokm\u00E5l");
	case Language::POLISH: return QString::fromUtf8(u8"Polski");
	case Language::PORTUGUESE: return QString::fromUtf8(u8"Portugu\u00EAs");
	case Language::PORTUGUESE_BRAZILIAN: return QString::fromUtf8(u8"Portugu\u00EAs (Brasil)");
	case Language::RUSSIAN: return QString::fromUtf8(u8"\u0420\u0443\u0441\u0441\u043A\u0438\u0439");
	case Language::SERBIAN: return QString::fromUtf8(u8"Srpski");
	case Language::SPANISH: return QString::fromUtf8(u8"Espa\u00F1ol");
	case Language::SWEDISH: return QString::fromUtf8(u8"Svenska");
	case Language::TURKISH: return QString::fromUtf8(u8"T\u00FCrk\u00E7e");
	default: return tr("<System Language>");
	}
}
