/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QWINDOWSFONTDATABASE_H
#define QWINDOWSFONTDATABASE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qpa/qplatformfontdatabase.h>
#include <QtCore/QSharedPointer>
#include <QtCore/QLoggingCategory>
#include <QtCore/qt_windows.h>

#if !defined(QT_NO_DIRECTWRITE)
    struct IDWriteFactory;
    struct IDWriteGdiInterop;
#endif

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQpaFonts)

class QWindowsFontEngineData
{
    Q_DISABLE_COPY_MOVE(QWindowsFontEngineData)
public:
    QWindowsFontEngineData();
    ~QWindowsFontEngineData();

    uint pow_gamma[256];

    bool clearTypeEnabled = false;
    qreal fontSmoothingGamma;
    HDC hdc = 0;
#if !defined(QT_NO_DIRECTWRITE)
    IDWriteFactory *directWriteFactory = nullptr;
    IDWriteGdiInterop *directWriteGdiInterop = nullptr;
#endif
};

class QWindowsFontDatabase : public QPlatformFontDatabase
{
    Q_DISABLE_COPY_MOVE(QWindowsFontDatabase)
public:
    enum FontOptions {
        // Relevant bits from QWindowsIntegration::Options
        DontUseDirectWriteFonts = 0x40,
        DontUseColorFonts = 0x80
    };

    QWindowsFontDatabase();
    ~QWindowsFontDatabase() override;

    void ensureFamilyPopulated(const QString &familyName);

    void populateFontDatabase() override;
    void populateFamily(const QString &familyName) override;
    QFontEngine *fontEngine(const QFontDef &fontDef, void *handle) override;
    QFontEngine *fontEngine(const QByteArray &fontData, qreal pixelSize, QFont::HintingPreference hintingPreference) override;
    QStringList fallbacksForFamily(const QString &family, QFont::Style style, QFont::StyleHint styleHint, QChar::Script script) const override;
    QStringList addApplicationFont(const QByteArray &fontData, const QString &fileName) override;
    void releaseHandle(void *handle) override;
    QString fontDir() const override;

    QFont defaultFont() const  override { return systemDefaultFont(); }
    bool fontsAlwaysScalable() const override;
    void derefUniqueFont(const QString &uniqueFont);
    void refUniqueFont(const QString &uniqueFont);
    bool isPrivateFontFamily(const QString &family) const override;

    static QFont systemDefaultFont();

    static QFontEngine *createEngine(const QFontDef &request, const QString &faceName,
                                     int dpi,
                                     const QSharedPointer<QWindowsFontEngineData> &data);

    static HFONT systemFont();
    static QFont LOGFONT_to_QFont(const LOGFONT& lf, int verticalDPI = 0);

    static qreal fontSmoothingGamma();
    static LOGFONT fontDefToLOGFONT(const QFontDef &fontDef, const QString &faceName);

    static QStringList extraTryFontsForFamily(const QString &family);
    static QString familyForStyleHint(QFont::StyleHint styleHint);

    static int defaultVerticalDPI();

    static void setFontOptions(unsigned options);
    static unsigned fontOptions();

private:
    void removeApplicationFonts();
    void addDefaultEUDCFont();

    struct WinApplicationFont {
        HANDLE handle;
        QString fileName;
    };

    QList<WinApplicationFont> m_applicationFonts;

    struct UniqueFontData {
        HANDLE handle;
        QAtomicInt refCount;
    };

    QMap<QString, UniqueFontData> m_uniqueFontData;

    static unsigned m_fontOptions;
    QStringList m_eudcFonts;
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug, const QFontDef &def);
#endif

inline quint16 qt_getUShort(const unsigned char *p)
{
    quint16 val;
    val = *p++ << 8;
    val |= *p;

    return val;
}

struct QFontNames
{
    QString name;   // e.g. "DejaVu Sans Condensed"
    QString style;  // e.g. "Italic"
    QString preferredName;  // e.g. "DejaVu Sans"
    QString preferredStyle; // e.g. "Condensed Italic"
};

struct QFontValues
{
    quint16 weight = 0;
    bool isItalic = false;
    bool isOverstruck = false;
    bool isUnderlined = false;
};

bool qt_localizedName(const QString &name);
QString qt_getEnglishName(const QString &familyName, bool includeStyle = false);
QFontNames qt_getCanonicalFontNames(const LOGFONT &lf);

QT_END_NAMESPACE

#endif // QWINDOWSFONTDATABASE_H
