/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QGRAPHICSEFFECT_P_H
#define QGRAPHICSEFFECT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qapplication_*.cpp, qwidget*.cpp and qfiledialog.cpp.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "qgraphicseffect.h"

#include <QPixmapCache>

#include <private/qobject_p.h>
#include <private/qpixmapfilter_p.h>

QT_REQUIRE_CONFIG(graphicseffect);

QT_BEGIN_NAMESPACE

class QGraphicsEffectSourcePrivate;
class Q_WIDGETS_EXPORT QGraphicsEffectSource : public QObject
{
    Q_OBJECT
public:
    ~QGraphicsEffectSource();
    const QGraphicsItem *graphicsItem() const;
    const QWidget *widget() const;
    const QStyleOption *styleOption() const;

    bool isPixmap() const;
    void draw(QPainter *painter);
    void update();

    QRectF boundingRect(Qt::CoordinateSystem coordinateSystem = Qt::LogicalCoordinates) const;
    QRect deviceRect() const;
    QPixmap pixmap(Qt::CoordinateSystem system = Qt::LogicalCoordinates,
                   QPoint *offset = nullptr,
                   QGraphicsEffect::PixmapPadMode mode = QGraphicsEffect::PadToEffectiveBoundingRect) const;

protected:
    QGraphicsEffectSource(QGraphicsEffectSourcePrivate &dd, QObject *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(QGraphicsEffectSource)
    Q_DISABLE_COPY_MOVE(QGraphicsEffectSource)
    friend class QGraphicsEffect;
    friend class QGraphicsEffectPrivate;
    friend class QGraphicsScenePrivate;
    friend class QGraphicsItem;
    friend class QGraphicsItemPrivate;
    friend class QWidget;
    friend class QWidgetPrivate;
};

class QGraphicsEffectSourcePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsEffectSource)
public:
    QGraphicsEffectSourcePrivate()
        : QObjectPrivate()
        , m_cachedSystem(Qt::DeviceCoordinates)
        , m_cachedMode(QGraphicsEffect::PadToTransparentBorder)
    {}

    enum InvalidateReason
    {
        TransformChanged,
        EffectRectChanged,
        SourceChanged
    };

    virtual ~QGraphicsEffectSourcePrivate();
    virtual void detach() = 0;
    virtual QRectF boundingRect(Qt::CoordinateSystem system) const = 0;
    virtual QRect deviceRect() const = 0;
    virtual const QGraphicsItem *graphicsItem() const = 0;
    virtual const QWidget *widget() const = 0;
    virtual const QStyleOption *styleOption() const = 0;
    virtual void draw(QPainter *p) = 0;
    virtual void update() = 0;
    virtual bool isPixmap() const = 0;
    virtual QPixmap pixmap(Qt::CoordinateSystem system, QPoint *offset = nullptr,
                           QGraphicsEffect::PixmapPadMode mode = QGraphicsEffect::PadToTransparentBorder) const = 0;
    virtual void effectBoundingRectChanged() = 0;

    void setCachedOffset(const QPoint &offset);
    void invalidateCache(InvalidateReason reason = SourceChanged) const;
    Qt::CoordinateSystem currentCachedSystem() const { return m_cachedSystem; }
    QGraphicsEffect::PixmapPadMode currentCachedMode() const { return m_cachedMode; }

    friend class QGraphicsScenePrivate;
    friend class QGraphicsItem;
    friend class QGraphicsItemPrivate;

private:
    mutable Qt::CoordinateSystem m_cachedSystem;
    mutable QGraphicsEffect::PixmapPadMode m_cachedMode;
    mutable QPoint m_cachedOffset;
    mutable QPixmapCache::Key m_cacheKey;
};

class Q_WIDGETS_EXPORT QGraphicsEffectPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsEffect)
public:
    QGraphicsEffectPrivate() : source(nullptr), isEnabled(1) {}
    ~QGraphicsEffectPrivate();

    inline void setGraphicsEffectSource(QGraphicsEffectSource *newSource)
    {
        QGraphicsEffect::ChangeFlags flags;
        if (source) {
            flags |= QGraphicsEffect::SourceDetached;
            source->d_func()->invalidateCache();
            source->d_func()->detach();
            delete source;
        }
        source = newSource;
        if (newSource)
            flags |= QGraphicsEffect::SourceAttached;
        q_func()->sourceChanged(flags);
    }

    QGraphicsEffectSource *source;
    QRectF boundingRect;
    quint32 isEnabled : 1;
    quint32 padding : 31; // feel free to use
};


class QGraphicsColorizeEffectPrivate : public QGraphicsEffectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsColorizeEffect)
public:
    QGraphicsColorizeEffectPrivate()
        : opaque(true)
    {
        filter = new QPixmapColorizeFilter;
    }
    ~QGraphicsColorizeEffectPrivate() { delete filter; }

    QPixmapColorizeFilter *filter;
    quint32 opaque : 1;
    quint32 padding : 31;
};

class QGraphicsBlurEffectPrivate : public QGraphicsEffectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsBlurEffect)
public:
    QGraphicsBlurEffectPrivate() : filter(new QPixmapBlurFilter) {}
    ~QGraphicsBlurEffectPrivate() { delete filter; }

    QPixmapBlurFilter *filter;
};

class QGraphicsDropShadowEffectPrivate : public QGraphicsEffectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsDropShadowEffect)
public:
    QGraphicsDropShadowEffectPrivate() : filter(new QPixmapDropShadowFilter) {}
    ~QGraphicsDropShadowEffectPrivate() { delete filter; }

    QPixmapDropShadowFilter *filter;
};

class QGraphicsOpacityEffectPrivate : public QGraphicsEffectPrivate
{
    Q_DECLARE_PUBLIC(QGraphicsOpacityEffect)
public:
    QGraphicsOpacityEffectPrivate()
        : opacity(qreal(0.7)), isFullyTransparent(0), isFullyOpaque(0), hasOpacityMask(0) {}
    ~QGraphicsOpacityEffectPrivate() {}

    qreal opacity;
    QBrush opacityMask;
    uint isFullyTransparent : 1;
    uint isFullyOpaque : 1;
    uint hasOpacityMask : 1;
};

QT_END_NAMESPACE

#endif // QGRAPHICSEFFECT_P_H
