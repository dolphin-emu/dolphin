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

#ifndef QSTATICTEXT_P_H
#define QSTATICTEXT_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include "qstatictext.h"

#include <private/qtextureglyphcache_p.h>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

// ### Qt 6: Unexport again, if QOpenGLStaticTextUserData (the one from QtOpenGL) is gone by then
class Q_GUI_EXPORT QStaticTextUserData
{
public:
    enum Type {
        NoUserData,
        OpenGLUserData
    };

    QStaticTextUserData(Type t) : ref(0), type(t) {}
    virtual ~QStaticTextUserData();

    QAtomicInt ref;
    Type type;
};

class Q_GUI_EXPORT QStaticTextItem
{
public:
    QStaticTextItem() : useBackendOptimizations(false),
                        userDataNeedsUpdate(0), usesRawFont(0),
                        m_fontEngine(nullptr), m_userData(nullptr) {}

    void setUserData(QStaticTextUserData *newUserData)
    {
        m_userData = newUserData;
    }
    QStaticTextUserData *userData() const { return m_userData.data(); }

    void setFontEngine(QFontEngine *fe)
    {
        m_fontEngine = fe;
    }

    QFontEngine *fontEngine() const { return m_fontEngine.data(); }

    union {
        QFixedPoint *glyphPositions;             // 8 bytes per glyph
        int positionOffset;
    };
    union {
        glyph_t *glyphs;                         // 4 bytes per glyph
        int glyphOffset;
    };
                                                 // =================
                                                 // 12 bytes per glyph

                                                 // 8 bytes for pointers
    int numGlyphs;                               // 4 bytes per item
    QFont font;                                  // 8 bytes per item
    QColor color;                                // 10 bytes per item
    char useBackendOptimizations : 1;            // 1 byte per item
    char userDataNeedsUpdate : 1;                //
    char usesRawFont : 1;                        //

private: // private to avoid abuse
    QExplicitlySharedDataPointer<QFontEngine> m_fontEngine;       // 4 bytes per item
    QExplicitlySharedDataPointer<QStaticTextUserData> m_userData; // 8 bytes per item
                                                                  // ================
                                                                  // 43 bytes per item
};
Q_DECLARE_TYPEINFO(QStaticTextItem, Q_MOVABLE_TYPE);

class QStaticText;
class Q_AUTOTEST_EXPORT QStaticTextPrivate
{
public:
    QStaticTextPrivate();
    QStaticTextPrivate(const QStaticTextPrivate &other);
    ~QStaticTextPrivate();

    void init();
    void paintText(const QPointF &pos, QPainter *p, const QColor &pen);

    void invalidate()
    {
        needsRelayout = true;
    }

    QAtomicInt ref;                      // 4 bytes per text

    QString text;                        // 4 bytes per text
    QFont font;                          // 8 bytes per text
    qreal textWidth;                     // 8 bytes per text
    QSizeF actualSize;                   // 16 bytes per text
    QPointF position;                    // 16 bytes per text

    QTransform matrix;                   // 80 bytes per text
    QStaticTextItem *items;              // 4 bytes per text
    int itemCount;                       // 4 bytes per text

    glyph_t *glyphPool;                  // 4 bytes per text
    QFixedPoint *positionPool;           // 4 bytes per text

    QTextOption textOption;              // 28 bytes per text

    unsigned char needsRelayout            : 1; // 1 byte per text
    unsigned char useBackendOptimizations  : 1;
    unsigned char textFormat               : 2;
    unsigned char untransformedCoordinates : 1;
                                         // ================
                                         // 191 bytes per text

    static QStaticTextPrivate *get(const QStaticText *q);
};

QT_END_NAMESPACE

#endif // QSTATICTEXT_P_H
