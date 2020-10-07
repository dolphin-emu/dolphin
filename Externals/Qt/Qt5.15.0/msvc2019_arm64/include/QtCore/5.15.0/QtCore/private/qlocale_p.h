/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QLOCALE_P_H
#define QLOCALE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/private/qglobal_p.h>
#include "QtCore/qstring.h"
#include "QtCore/qvarlengtharray.h"
#include "QtCore/qvariant.h"
#include "QtCore/qnumeric.h"
#include <QtCore/qcalendar.h>

#include "qlocale.h"

#include <limits>
#include <cmath>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_SYSTEMLOCALE
struct QLocaleData;
class Q_CORE_EXPORT QSystemLocale
{
public:
    QSystemLocale();
    virtual ~QSystemLocale();

    struct CurrencyToStringArgument
    {
        CurrencyToStringArgument() { }
        CurrencyToStringArgument(const QVariant &v, const QString &s)
            : value(v), symbol(s) { }
        QVariant value;
        QString symbol;
    };

    enum QueryType {
        LanguageId, // uint
        CountryId, // uint
        DecimalPoint, // QString
        GroupSeparator, // QString (empty QString means: don't group digits)
        ZeroDigit, // QString
        NegativeSign, // QString
        DateFormatLong, // QString
        DateFormatShort, // QString
        TimeFormatLong, // QString
        TimeFormatShort, // QString
        DayNameLong, // QString, in: int
        DayNameShort, // QString, in: int
        MonthNameLong, // QString, in: int
        MonthNameShort, // QString, in: int
        DateToStringLong, // QString, in: QDate
        DateToStringShort, // QString in: QDate
        TimeToStringLong, // QString in: QTime
        TimeToStringShort, // QString in: QTime
        DateTimeFormatLong, // QString
        DateTimeFormatShort, // QString
        DateTimeToStringLong, // QString in: QDateTime
        DateTimeToStringShort, // QString in: QDateTime
        MeasurementSystem, // uint
        PositiveSign, // QString
        AMText, // QString
        PMText, // QString
        FirstDayOfWeek, // Qt::DayOfWeek
        Weekdays, // QList<Qt::DayOfWeek>
        CurrencySymbol, // QString in: CurrencyToStringArgument
        CurrencyToString, // QString in: qlonglong, qulonglong or double
        Collation, // QString
        UILanguages, // QStringList
        StringToStandardQuotation, // QString in: QStringRef to quote
        StringToAlternateQuotation, // QString in: QStringRef to quote
        ScriptId, // uint
        ListToSeparatedString, // QString
        LocaleChanged, // system locale changed
        NativeLanguageName, // QString
        NativeCountryName, // QString
        StandaloneMonthNameLong, // QString, in: int
        StandaloneMonthNameShort // QString, in: int
    };
    virtual QVariant query(QueryType type, QVariant in) const;
    virtual QLocale fallbackUiLocale() const;

    inline const QLocaleData *fallbackUiLocaleData() const;
private:
    QSystemLocale(bool);
    friend class QSystemLocaleSingleton;
};
Q_DECLARE_TYPEINFO(QSystemLocale::QueryType, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(QSystemLocale::CurrencyToStringArgument, Q_MOVABLE_TYPE);
#endif

#if QT_CONFIG(icu)
namespace QIcu {
    QString toUpper(const QByteArray &localeId, const QString &str, bool *ok);
    QString toLower(const QByteArray &localeId, const QString &str, bool *ok);
}
#endif


struct QLocaleId
{
    // bypass constructors
    static inline QLocaleId fromIds(ushort language, ushort script, ushort country)
    {
        const QLocaleId localeId = { language, script, country };
        return localeId;
    }

    inline bool operator==(QLocaleId other) const
    { return language_id == other.language_id && script_id == other.script_id && country_id == other.country_id; }
    inline bool operator!=(QLocaleId other) const
    { return !operator==(other); }

    QLocaleId withLikelySubtagsAdded() const;
    QLocaleId withLikelySubtagsRemoved() const;

    QByteArray name(char separator = '-') const;

    ushort language_id, script_id, country_id;
};
Q_DECLARE_TYPEINFO(QLocaleId, Q_PRIMITIVE_TYPE);

struct QLocaleData
{
public:
    // TODO: Remove this?
    static const QLocaleData *findLocaleData(QLocale::Language language,
                                             QLocale::Script script,
                                             QLocale::Country country);
    // Having an offset of current locale, enables us to have multiple sources of data, i.e. user-provided calendar locales
    static uint findLocaleOffset(QLocale::Language language,
                                 QLocale::Script script,
                                 QLocale::Country country);
    static const QLocaleData *c();

    // Maximum number of significant digits needed to represent a double.
    // We cannot use std::numeric_limits here without constexpr.
    static const int DoubleMantissaBits = 53;
    static const int Log10_2_100000 = 30103;    // log10(2) * 100000
    // same as C++11 std::numeric_limits<T>::max_digits10
    static const int DoubleMaxSignificant = (DoubleMantissaBits * Log10_2_100000) / 100000 + 2;

    // Maximum number of digits before decimal point to represent a double
    // Same as std::numeric_limits<double>::max_exponent10 + 1
    static const int DoubleMaxDigitsBeforeDecimal = 309;

    enum DoubleForm {
        DFExponent = 0,
        DFDecimal,
        DFSignificantDigits,
        _DFMax = DFSignificantDigits
    };

    enum Flags {
        NoFlags             = 0,
        AddTrailingZeroes   = 0x01,
        ZeroPadded          = 0x02,
        LeftAdjusted        = 0x04,
        BlankBeforePositive = 0x08,
        AlwaysShowSign      = 0x10,
        ThousandsGroup      = 0x20,
        CapitalEorX         = 0x40,

        ShowBase            = 0x80,
        UppercaseBase       = 0x100,
        ZeroPadExponent     = 0x200,
        ForcePoint          = 0x400,
        IndianNumberGrouping= 0x800
    };

    enum NumberMode { IntegerMode, DoubleStandardMode, DoubleScientificMode };

    typedef QVarLengthArray<char, 256> CharBuff;

    static QString doubleToString(const QChar zero, const QChar plus,
                                  const QChar minus, const QChar exponent,
                                  const QChar group, const QChar decimal,
                                  double d, int precision,
                                  DoubleForm form,
                                  int width, unsigned flags);
    static QString longLongToString(const QChar zero, const QChar group,
                                    const QChar plus, const QChar minus,
                                    qint64 l, int precision, int base,
                                    int width, unsigned flags);
    static QString unsLongLongToString(const QChar zero, const QChar group,
                                       const QChar plus,
                                       quint64 l, int precision,
                                       int base, int width,
                                       unsigned flags);

    QString doubleToString(double d,
                           int precision = -1,
                           DoubleForm form = DFSignificantDigits,
                           int width = -1,
                           unsigned flags = NoFlags) const;
    QString longLongToString(qint64 l, int precision = -1,
                             int base = 10,
                             int width = -1,
                             unsigned flags = NoFlags) const;
    QString unsLongLongToString(quint64 l, int precision = -1,
                                int base = 10,
                                int width = -1,
                                unsigned flags = NoFlags) const;

    // this function is meant to be called with the result of stringToDouble or bytearrayToDouble
    static float convertDoubleToFloat(double d, bool *ok)
    {
        if (qIsInf(d))
            return float(d);
        if (std::fabs(d) > std::numeric_limits<float>::max()) {
            if (ok)
                *ok = false;
            const float huge = std::numeric_limits<float>::infinity();
            return d < 0 ? -huge : huge;
        }
        if (d != 0 && float(d) == 0) {
            // Values that underflow double already failed. Match them:
            if (ok)
                *ok = false;
            return 0;
        }
        return float(d);
    }

    double stringToDouble(QStringView str, bool *ok, QLocale::NumberOptions options) const;
    qint64 stringToLongLong(QStringView str, int base, bool *ok, QLocale::NumberOptions options) const;
    quint64 stringToUnsLongLong(QStringView str, int base, bool *ok, QLocale::NumberOptions options) const;

    static double bytearrayToDouble(const char *num, bool *ok);
    // this function is used in QIntValidator (QtGui)
    Q_CORE_EXPORT static qint64 bytearrayToLongLong(const char *num, int base, bool *ok);
    static quint64 bytearrayToUnsLongLong(const char *num, int base, bool *ok);

    bool numberToCLocale(QStringView s, QLocale::NumberOptions number_options,
                         CharBuff *result) const;
    inline char digitToCLocale(QChar c) const;

    // this function is used in QIntValidator (QtGui)
    Q_CORE_EXPORT bool validateChars(QStringView str, NumberMode numMode, QByteArray *buff, int decDigits = -1,
            QLocale::NumberOptions number_options = QLocale::DefaultNumberOptions) const;

public:
    quint16 m_language_id, m_script_id, m_country_id;

    // FIXME QTBUG-69324: not all unicode code-points map to single-token UTF-16 :-(
    char16_t m_decimal, m_group, m_list, m_percent, m_zero, m_minus, m_plus, m_exponential;
    char16_t m_quotation_start, m_quotation_end;
    char16_t m_alternate_quotation_start, m_alternate_quotation_end;

    quint16 m_list_pattern_part_start_idx, m_list_pattern_part_start_size;
    quint16 m_list_pattern_part_mid_idx, m_list_pattern_part_mid_size;
    quint16 m_list_pattern_part_end_idx, m_list_pattern_part_end_size;
    quint16 m_list_pattern_part_two_idx, m_list_pattern_part_two_size;
    quint16 m_short_date_format_idx, m_short_date_format_size;
    quint16 m_long_date_format_idx, m_long_date_format_size;
    quint16 m_short_time_format_idx, m_short_time_format_size;
    quint16 m_long_time_format_idx, m_long_time_format_size;
    quint16 m_standalone_short_day_names_idx, m_standalone_short_day_names_size;
    quint16 m_standalone_long_day_names_idx, m_standalone_long_day_names_size;
    quint16 m_standalone_narrow_day_names_idx, m_standalone_narrow_day_names_size;
    quint16 m_short_day_names_idx, m_short_day_names_size;
    quint16 m_long_day_names_idx, m_long_day_names_size;
    quint16 m_narrow_day_names_idx, m_narrow_day_names_size;
    quint16 m_am_idx, m_am_size;
    quint16 m_pm_idx, m_pm_size;
    quint16 m_byte_idx, m_byte_size;
    quint16 m_byte_si_quantified_idx, m_byte_si_quantified_size;
    quint16 m_byte_iec_quantified_idx, m_byte_iec_quantified_size;
    char    m_currency_iso_code[3];
    quint16 m_currency_symbol_idx, m_currency_symbol_size;
    quint16 m_currency_display_name_idx, m_currency_display_name_size;
    quint8  m_currency_format_idx, m_currency_format_size;
    quint8  m_currency_negative_format_idx, m_currency_negative_format_size;
    quint16 m_language_endonym_idx, m_language_endonym_size;
    quint16 m_country_endonym_idx, m_country_endonym_size;
    quint16 m_currency_digits : 2;
    quint16 m_currency_rounding : 3;
    quint16 m_first_day_of_week : 3;
    quint16 m_weekend_start : 3;
    quint16 m_weekend_end : 3;
};

class Q_CORE_EXPORT QLocalePrivate // A POD type
{
public:
    static QLocalePrivate *create(
            const QLocaleData *data, const uint data_offset = 0,
            QLocale::NumberOptions numberOptions = QLocale::DefaultNumberOptions)
    {
        auto *retval = new QLocalePrivate;
        retval->m_data = data;
        retval->ref.storeRelaxed(0);
        retval->m_data_offset = data_offset;
        retval->m_numberOptions = numberOptions;
        return retval;
    }

    static QLocalePrivate *get(QLocale &l) { return l.d; }
    static const QLocalePrivate *get(const QLocale &l) { return l.d; }

    QChar decimal() const { return QChar(m_data->m_decimal); }
    QChar group() const { return QChar(m_data->m_group); }
    QChar list() const { return QChar(m_data->m_list); }
    QChar percent() const { return QChar(m_data->m_percent); }
    QChar zero() const { return QChar(m_data->m_zero); }
    QChar plus() const { return QChar(m_data->m_plus); }
    QChar minus() const { return QChar(m_data->m_minus); }
    QChar exponential() const { return QChar(m_data->m_exponential); }

    quint16 languageId() const { return m_data->m_language_id; }
    quint16 countryId() const { return m_data->m_country_id; }

    QByteArray bcp47Name(char separator = '-') const;
    QByteArray rawName(char separator = '-') const;

    inline QLatin1String languageCode() const { return languageToCode(QLocale::Language(m_data->m_language_id)); }
    inline QLatin1String scriptCode() const { return scriptToCode(QLocale::Script(m_data->m_script_id)); }
    inline QLatin1String countryCode() const { return countryToCode(QLocale::Country(m_data->m_country_id)); }

    static QLatin1String languageToCode(QLocale::Language language);
    static QLatin1String scriptToCode(QLocale::Script script);
    static QLatin1String countryToCode(QLocale::Country country);
    static QLocale::Language codeToLanguage(QStringView code) noexcept;
    static QLocale::Script codeToScript(QStringView code) noexcept;
    static QLocale::Country codeToCountry(QStringView code) noexcept;
    static void getLangAndCountry(const QString &name, QLocale::Language &lang,
                                  QLocale::Script &script, QLocale::Country &cntry);

    QLocale::MeasurementSystem measurementSystem() const;

    const QLocaleData *m_data;
    QBasicAtomicInt ref;
    uint m_data_offset;
    QLocale::NumberOptions m_numberOptions;
};

#ifndef QT_NO_SYSTEMLOCALE
const QLocaleData *QSystemLocale::fallbackUiLocaleData() const { return fallbackUiLocale().d->m_data; }
#endif

template <>
inline QLocalePrivate *QSharedDataPointer<QLocalePrivate>::clone()
{
    // cannot use QLocalePrivate's copy constructor
    // since it is deleted in C++11
    return QLocalePrivate::create(d->m_data, d->m_data_offset, d->m_numberOptions);
}

inline char QLocaleData::digitToCLocale(QChar in) const
{
    const ushort tenUnicode = m_zero + 10;

    if (in.unicode() >= m_zero && in.unicode() < tenUnicode)
        return '0' + in.unicode() - m_zero;

    if (in.unicode() >= '0' && in.unicode() <= '9')
        return in.toLatin1();

    if (in == m_plus || in == QLatin1Char('+'))
        return '+';

    if (in == m_minus || in == QLatin1Char('-') || in == QChar(0x2212))
        return '-';

    if (in == m_decimal)
        return '.';

    if (in == m_group)
        return ',';

    if (in == m_exponential || in == QChar(QChar::toUpper(m_exponential)))
        return 'e';

    // In several languages group() is a non-breaking space (U+00A0) or its thin
    // version (U+202f), which look like spaces.  People (and thus some of our
    // tests) use a regular space instead and complain if it doesn't work.
    if ((m_group == 0xA0 || m_group == 0x202f) && in.unicode() == ' ')
        return ',';

    return 0;
}

QString qt_readEscapedFormatString(QStringView format, int *idx);
bool qt_splitLocaleName(const QString &name, QString &lang, QString &script, QString &cntry);
int qt_repeatCount(QStringView s);

enum { AsciiSpaceMask = (1u << (' ' - 1)) |
                        (1u << ('\t' - 1)) |   // 9: HT - horizontal tab
                        (1u << ('\n' - 1)) |   // 10: LF - line feed
                        (1u << ('\v' - 1)) |   // 11: VT - vertical tab
                        (1u << ('\f' - 1)) |   // 12: FF - form feed
                        (1u << ('\r' - 1)) };  // 13: CR - carriage return
Q_DECL_CONSTEXPR inline bool ascii_isspace(uchar c)
{
    return c >= 1u && c <= 32u && (AsciiSpaceMask >> uint(c - 1)) & 1u;
}

#if defined(Q_COMPILER_CONSTEXPR)
Q_STATIC_ASSERT(ascii_isspace(' '));
Q_STATIC_ASSERT(ascii_isspace('\t'));
Q_STATIC_ASSERT(ascii_isspace('\n'));
Q_STATIC_ASSERT(ascii_isspace('\v'));
Q_STATIC_ASSERT(ascii_isspace('\f'));
Q_STATIC_ASSERT(ascii_isspace('\r'));
Q_STATIC_ASSERT(!ascii_isspace('\0'));
Q_STATIC_ASSERT(!ascii_isspace('\a'));
Q_STATIC_ASSERT(!ascii_isspace('a'));
Q_STATIC_ASSERT(!ascii_isspace('\177'));
Q_STATIC_ASSERT(!ascii_isspace(uchar('\200')));
Q_STATIC_ASSERT(!ascii_isspace(uchar('\xA0')));
Q_STATIC_ASSERT(!ascii_isspace(uchar('\377')));
#endif

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QStringRef)
Q_DECLARE_METATYPE(QList<Qt::DayOfWeek>)
#ifndef QT_NO_SYSTEMLOCALE
Q_DECLARE_METATYPE(QSystemLocale::CurrencyToStringArgument)
#endif

#endif // QLOCALE_P_H
