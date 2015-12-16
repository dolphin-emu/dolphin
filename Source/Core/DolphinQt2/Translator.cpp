// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <clocale>
#include <libintl.h>
#include <QString>

#include "Core/ConfigManager.h"
#include "DolphinQt2/Translator.h"

void Translator::Init()
{
	bindtextdomain("dolphin-emu", DATA_DIR "../locale");
	textdomain("dolphin-emu");
	setlocale(LC_ALL, "");

	Language language_id = static_cast<Language>(SConfig::GetInstance().m_InterfaceLanguage);
	if (language_id != Language::DEFAULT)
		SetLanguage(language_id);

	RegisterStringTranslator([] (const char* string) -> std::string {return gettext(string);});
}

void Translator::SetLanguage(Language language_id)
{
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
	case Language::CATALAN: return "ca_ES.UTF-8";
	case Language::CZECH: return "cs_CZ.UTF-8";
	case Language::GERMAN: return "de_DE.UTF-8";
	case Language::ENGLISH: return "en_GB.UTF-8";
	case Language::SPANISH: return "es_ES.UTF-8";
	case Language::FRENCH: return "fr_FR.UTF-8";
	case Language::ITALIAN: return "it_IT.UTF-8";
	case Language::HUNGARIAN: return "hu_HU.UTF-8";
	case Language::DUTCH: return "nl_NL.UTF-8";
	case Language::NORWEGIAN_BOKMAL: return "nb_NO.UTF-8";
	case Language::POLISH: return "pl_PL.UTF-8";
	case Language::PORTUGUESE: return "pt_PT.UTF-8";
	case Language::PORTUGUESE_BRAZILIAN: return "pt_BR.UTF-8";
	case Language::SERBIAN: return "sr_RS.UTF-8";
	case Language::SWEDISH: return "sv_SE.UTF-8";
	case Language::TURKISH: return "tr_TR.UTF-8";
	case Language::GREEK: return "el_GR.UTF-8";
	case Language::RUSSIAN: return "ru_RU.UTF-8";
	case Language::HEBREW: return "he_IL.UTF-8";
	case Language::ARABIC: return "ar.UTF-8";
	case Language::FARSI: return "fa_IR.UTF-8";
	case Language::KOREAN: return "ko_KR.UTF-8";
	case Language::JAPANESE: return "ja_JP.UTF-8";
	case Language::CHINESE_SIMPLIFIED: return "zh_CN.UTF-8";
	case Language::CHINESE_TRADITIONAL: return "zh_TW.UTF-8";
	default: return ""; //setlocale() interprets the empty string as the system locale
	}
}

//map enum to native language name
const QString Translator::DisplayLanguage(Language language_id)
{
	switch (language_id)
	{
	case Language::CATALAN: return QStringLiteral("Catal\u00E0");
	case Language::CZECH: return QStringLiteral("\u010Ce\u0161tina");
	case Language::GERMAN: return QStringLiteral("Deutsch");
	case Language::ENGLISH: return QStringLiteral("English");
	case Language::SPANISH: return QStringLiteral("Espa\u00F1ol");
	case Language::FRENCH: return QStringLiteral("Fran\u00E7ais");
	case Language::ITALIAN: return QStringLiteral("Italiano");
	case Language::HUNGARIAN: return QStringLiteral("Magyar");
	case Language::DUTCH: return QStringLiteral("Nederlands");
	case Language::NORWEGIAN_BOKMAL: return QStringLiteral("Norsk bokm\u00E5l");
	case Language::POLISH: return QStringLiteral("Polski");
	case Language::PORTUGUESE: return QStringLiteral("Portugu\u00EAs");
	case Language::PORTUGUESE_BRAZILIAN: return QStringLiteral("Portugu\u00EAs (Brasil)");
	case Language::SERBIAN: return QStringLiteral("Srpski");
	case Language::SWEDISH: return QStringLiteral("Svenska");
	case Language::TURKISH: return QStringLiteral("T\u00FCrk\u00E7e");
	case Language::GREEK: return QStringLiteral("\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC");
	case Language::RUSSIAN: return QStringLiteral("\u0420\u0443\u0441\u0441\u043A\u0438\u0439");
	case Language::HEBREW: return QStringLiteral("\u05E2\u05D1\u05E8\u05D9\u05EA");
	case Language::ARABIC: return QStringLiteral("\u0627\u0644\u0639\u0631\u0628\u064A\u0629");
	case Language::FARSI: return QStringLiteral("\u0641\u0627\u0631\u0633\u06CC");
	case Language::KOREAN: return QStringLiteral("\uD55C\uAD6D\uC5B4");
	case Language::JAPANESE: return QStringLiteral("\u65E5\u672C\u8A9E");
	case Language::CHINESE_SIMPLIFIED: return QStringLiteral("\u7B80\u4F53\u4E2D\u6587");
	case Language::CHINESE_TRADITIONAL: return QStringLiteral("\u7E41\u9AD4\u4E2D\u6587");
	default: return tr("<System Language>");
	}
}
