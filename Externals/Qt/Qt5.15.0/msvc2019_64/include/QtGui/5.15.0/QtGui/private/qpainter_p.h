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

#ifndef QPAINTER_P_H
#define QPAINTER_P_H

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

#include <QtCore/qvarlengtharray.h>
#include <QtGui/private/qtguiglobal_p.h>
#include "QtGui/qbrush.h"
#include "QtGui/qcolorspace.h"
#include "QtGui/qcolortransform.h"
#include "QtGui/qfont.h"
#include "QtGui/qpen.h"
#include "QtGui/qregion.h"
#include "QtGui/qpainter.h"
#include "QtGui/qpainterpath.h"
#include "QtGui/qpaintengine.h"

#include <private/qpen_p.h>

QT_BEGIN_NAMESPACE

class QPaintEngine;
class QEmulationPaintEngine;
class QPaintEngineEx;
struct QFixedPoint;

struct QTLWExtra;

struct DataPtrContainer {
    void *ptr;
};

inline const void *data_ptr(const QTransform &t) { return (const DataPtrContainer *) &t; }
inline bool qtransform_fast_equals(const QTransform &a, const QTransform &b) { return data_ptr(a) == data_ptr(b); }

// QPen inline functions...
inline QPen::DataPtr &data_ptr(const QPen &p) { return const_cast<QPen &>(p).data_ptr(); }
inline bool qpen_fast_equals(const QPen &a, const QPen &b) { return data_ptr(a) == data_ptr(b); }
inline QBrush qpen_brush(const QPen &p) { return data_ptr(p)->brush; }
inline qreal qpen_widthf(const QPen &p) { return data_ptr(p)->width; }
inline Qt::PenStyle qpen_style(const QPen &p) { return data_ptr(p)->style; }
inline Qt::PenCapStyle qpen_capStyle(const QPen &p) { return data_ptr(p)->capStyle; }
inline Qt::PenJoinStyle qpen_joinStyle(const QPen &p) { return data_ptr(p)->joinStyle; }

// QBrush inline functions...
inline QBrush::DataPtr &data_ptr(const QBrush &p) { return const_cast<QBrush &>(p).data_ptr(); }
inline bool qbrush_fast_equals(const QBrush &a, const QBrush &b) { return data_ptr(a) == data_ptr(b); }
inline Qt::BrushStyle qbrush_style(const QBrush &b) { return data_ptr(b)->style; }
inline const QColor &qbrush_color(const QBrush &b) { return data_ptr(b)->color; }
inline bool qbrush_has_transform(const QBrush &b) { return data_ptr(b)->transform.type() > QTransform::TxNone; }

class QPainterClipInfo
{
public:
    QPainterClipInfo() {} // for QVector, don't use
    enum ClipType { RegionClip, PathClip, RectClip, RectFClip };

    QPainterClipInfo(const QPainterPath &p, Qt::ClipOperation op, const QTransform &m) :
        clipType(PathClip), matrix(m), operation(op), path(p) { }

    QPainterClipInfo(const QRegion &r, Qt::ClipOperation op, const QTransform &m) :
        clipType(RegionClip), matrix(m), operation(op), region(r) { }

    QPainterClipInfo(const QRect &r, Qt::ClipOperation op, const QTransform &m) :
        clipType(RectClip), matrix(m), operation(op), rect(r) { }

    QPainterClipInfo(const QRectF &r, Qt::ClipOperation op, const QTransform &m) :
        clipType(RectFClip), matrix(m), operation(op), rectf(r) { }

    ClipType clipType;
    QTransform matrix;
    Qt::ClipOperation operation;
    QPainterPath path;
    QRegion region;
    QRect rect;
    QRectF rectf;

    // ###
//     union {
//         QRegionData *d;
//         QPainterPathPrivate *pathData;

//         struct {
//             int x, y, w, h;
//         } rectData;
//         struct {
//             qreal x, y, w, h;
//         } rectFData;
//     };

};

Q_DECLARE_TYPEINFO(QPainterClipInfo, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QPainterState : public QPaintEngineState
{
public:
    QPainterState();
    QPainterState(const QPainterState *s);
    virtual ~QPainterState();
    void init(QPainter *p);

    QPointF brushOrigin;
    QFont font;
    QFont deviceFont;
    QPen pen;
    QBrush brush;
    QBrush bgBrush = Qt::white; // background brush
    QRegion clipRegion;
    QPainterPath clipPath;
    Qt::ClipOperation clipOperation = Qt::NoClip;
    QPainter::RenderHints renderHints;
    QVector<QPainterClipInfo> clipInfo; // ### Make me smaller and faster to copy around...
    QTransform worldMatrix;       // World transformation matrix, not window and viewport
    QTransform matrix;            // Complete transformation matrix,
    QTransform redirectionMatrix;
    int wx = 0, wy = 0, ww = 0, wh = 0; // window rectangle
    int vx = 0, vy = 0, vw = 0, vh = 0; // viewport rectangle
    qreal opacity = 1;

    uint WxF:1;                 // World transformation
    uint VxF:1;                 // View transformation
    uint clipEnabled:1;

    Qt::BGMode bgMode = Qt::TransparentMode;
    QPainter *painter = nullptr;
    Qt::LayoutDirection layoutDirection;
    QPainter::CompositionMode composition_mode = QPainter::CompositionMode_SourceOver;
    uint emulationSpecifier = 0;
    uint changeFlags = 0;
};

struct QPainterDummyState
{
    QFont font;
    QPen pen;
    QBrush brush;
    QTransform transform;
};

class QRawFont;
class QPainterPrivate
{
    Q_DECLARE_PUBLIC(QPainter)
public:
    QPainterPrivate(QPainter *painter)
    : q_ptr(painter), d_ptrs(nullptr), state(nullptr), dummyState(nullptr), txinv(0), inDestructor(false), d_ptrs_size(0),
        refcount(1), device(nullptr), original_device(nullptr), helper_device(nullptr), engine(nullptr), emulationEngine(nullptr),
        extended(nullptr)
    {
    }

    ~QPainterPrivate();

    QPainter *q_ptr;
    QPainterPrivate **d_ptrs;

    QPainterState *state;
    QVarLengthArray<QPainterState *, 8> states;

    mutable QPainterDummyState *dummyState;

    QTransform invMatrix;
    uint txinv:1;
    uint inDestructor : 1;
    uint d_ptrs_size;
    uint refcount;

    enum DrawOperation { StrokeDraw        = 0x1,
                         FillDraw          = 0x2,
                         StrokeAndFillDraw = 0x3
    };

    QPainterDummyState *fakeState() const {
        if (!dummyState)
            dummyState = new QPainterDummyState();
        return dummyState;
    }

    void updateEmulationSpecifier(QPainterState *s);
    void updateStateImpl(QPainterState *state);
    void updateState(QPainterState *state);

    void draw_helper(const QPainterPath &path, DrawOperation operation = StrokeAndFillDraw);
    void drawStretchedGradient(const QPainterPath &path, DrawOperation operation);
    void drawOpaqueBackground(const QPainterPath &path, DrawOperation operation);
    void drawTextItem(const QPointF &p, const QTextItem &_ti, QTextEngine *textEngine);

#if !defined(QT_NO_RAWFONT)
    void drawGlyphs(const quint32 *glyphArray, QFixedPoint *positionArray, int glyphCount,
                    QFontEngine *fontEngine, bool overline = false, bool underline = false,
                    bool strikeOut = false);
#endif

    void updateMatrix();
    void updateInvMatrix();

    void checkEmulation();

    static QPainterPrivate *get(QPainter *painter)
    {
        return painter->d_ptr.data();
    }

    QTransform viewTransform() const;
    qreal effectiveDevicePixelRatio() const;
    QTransform hidpiScaleTransform() const;
    static bool attachPainterPrivate(QPainter *q, QPaintDevice *pdev);
    void detachPainterPrivate(QPainter *q);
    void initFrom(const QPaintDevice *device);

    QPaintDevice *device;
    QPaintDevice *original_device;
    QPaintDevice *helper_device;
    QPaintEngine *engine;
    QEmulationPaintEngine *emulationEngine;
    QPaintEngineEx *extended;
    QBrush colorBrush;          // for fill with solid color
};

Q_GUI_EXPORT void qt_draw_helper(QPainterPrivate *p, const QPainterPath &path, QPainterPrivate::DrawOperation operation);

QString qt_generate_brush_key(const QBrush &brush);

inline bool qt_pen_is_cosmetic(const QPen &pen, QPainter::RenderHints hints)
{
    return pen.isCosmetic() || (const_cast<QPen &>(pen).data_ptr()->defaultWidth && (hints & QPainter::Qt4CompatiblePainting));
}

QT_END_NAMESPACE

#endif // QPAINTER_P_H
