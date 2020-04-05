/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QDISTANCEFIELD_H
#define QDISTANCEFIELD_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>
#include <qrawfont.h>
#include <private/qfontengine_p.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qglobal.h>
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

bool Q_GUI_EXPORT qt_fontHasNarrowOutlines(const QRawFont &f);
bool Q_GUI_EXPORT qt_fontHasNarrowOutlines(QFontEngine *fontEngine);

int Q_GUI_EXPORT QT_DISTANCEFIELD_BASEFONTSIZE(bool narrowOutlineFont);
int Q_GUI_EXPORT QT_DISTANCEFIELD_TILESIZE(bool narrowOutlineFont);
int Q_GUI_EXPORT QT_DISTANCEFIELD_SCALE(bool narrowOutlineFont);
int Q_GUI_EXPORT QT_DISTANCEFIELD_RADIUS(bool narrowOutlineFont);
int Q_GUI_EXPORT QT_DISTANCEFIELD_HIGHGLYPHCOUNT();

class Q_GUI_EXPORT QDistanceFieldData : public QSharedData
{
public:
    QDistanceFieldData() : glyph(0), width(0), height(0), nbytes(0), data(nullptr) {}
    QDistanceFieldData(const QDistanceFieldData &other);
    ~QDistanceFieldData();

    static QDistanceFieldData *create(const QSize &size);
    static QDistanceFieldData *create(const QPainterPath &path, bool doubleResolution);

    glyph_t glyph;
    int width;
    int height;
    int nbytes;
    uchar *data;
};

class Q_GUI_EXPORT QDistanceField
{
public:
    QDistanceField();
    QDistanceField(int width, int height);
    QDistanceField(const QRawFont &font, glyph_t glyph, bool doubleResolution = false);
    QDistanceField(QFontEngine *fontEngine, glyph_t glyph, bool doubleResolution = false);
    QDistanceField(const QPainterPath &path, glyph_t glyph, bool doubleResolution = false);

    bool isNull() const;

    glyph_t glyph() const;
    void setGlyph(const QRawFont &font, glyph_t glyph, bool doubleResolution = false);
    void setGlyph(QFontEngine *fontEngine, glyph_t glyph, bool doubleResolution = false);

    int width() const;
    int height() const;

    QDistanceField copy(const QRect &rect = QRect()) const;
    inline QDistanceField copy(int x, int y, int w, int h) const
        { return copy(QRect(x, y, w, h)); }

    uchar *bits();
    const uchar *bits() const;
    const uchar *constBits() const;

    uchar *scanLine(int);
    const uchar *scanLine(int) const;
    const uchar *constScanLine(int) const;

    QImage toImage(QImage::Format format = QImage::Format_ARGB32_Premultiplied) const;

private:
    QDistanceField(QDistanceFieldData *data);
    QSharedDataPointer<QDistanceFieldData> d;

    friend class QDistanceFieldData;
};

QT_END_NAMESPACE

#endif // QDISTANCEFIELD_H
