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

#ifndef QHIGHDPISCALING_P_H
#define QHIGHDPISCALING_P_H

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
#include <QtCore/qmargins.h>
#include <QtCore/qmath.h>
#include <QtCore/qrect.h>
#include <QtCore/qvector.h>
#include <QtCore/qloggingcategory.h>
#include <QtGui/qregion.h>
#include <QtGui/qscreen.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qwindow.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScaling);

class QScreen;
class QPlatformScreen;
typedef QPair<qreal, qreal> QDpi;

#ifndef QT_NO_HIGHDPISCALING
class Q_GUI_EXPORT QHighDpiScaling {
    Q_GADGET
public:
    enum class DpiAdjustmentPolicy {
        Unset,
        Enabled,
        Disabled,
        UpOnly
    };
    Q_ENUM(DpiAdjustmentPolicy)

    QHighDpiScaling() = delete;
    ~QHighDpiScaling() = delete;
    QHighDpiScaling(const QHighDpiScaling &) = delete;
    QHighDpiScaling &operator=(const QHighDpiScaling &) = delete;
    QHighDpiScaling(QHighDpiScaling &&) = delete;
    QHighDpiScaling &operator=(QHighDpiScaling &&) = delete;

    static void initHighDpiScaling();
    static void updateHighDpiScaling();
    static void setGlobalFactor(qreal factor);
    static void setScreenFactor(QScreen *screen, qreal factor);

    static bool isActive() { return m_active; }

    struct ScaleAndOrigin
    {
        qreal factor;
        QPoint origin;
    };
    static ScaleAndOrigin scaleAndOrigin(const QPlatformScreen *platformScreen, QPoint *nativePosition = nullptr);
    static ScaleAndOrigin scaleAndOrigin(const QScreen *screen, QPoint *nativePosition = nullptr);
    static ScaleAndOrigin scaleAndOrigin(const QWindow *platformScreen, QPoint *nativePosition = nullptr);

    template<typename C>
    static qreal factor(C *context, QPoint *nativePosition = nullptr) {
        return scaleAndOrigin(context, nativePosition).factor;
    }

    static QPoint mapPositionFromNative(const QPoint &pos, const QPlatformScreen *platformScreen);
    static QPoint mapPositionToNative(const QPoint &pos, const QPlatformScreen *platformScreen);
    static QPoint mapPositionToGlobal(const QPoint &pos, const QPoint &windowGlobalPosition, const QWindow *window);
    static QPoint mapPositionFromGlobal(const QPoint &pos, const QPoint &windowGlobalPosition, const QWindow *window);
    static QDpi logicalDpi(const QScreen *screen);

private:
    static qreal rawScaleFactor(const QPlatformScreen *screen);
    static qreal roundScaleFactor(qreal rawFactor);
    static QDpi effectiveLogicalDpi(const QPlatformScreen *screen, qreal rawFactor, qreal roundedFactor);
    static qreal screenSubfactor(const QPlatformScreen *screen);

    static qreal m_factor;
    static bool m_active;
    static bool m_usePixelDensity;
    static bool m_globalScalingActive;
    static bool m_pixelDensityScalingActive;
    static bool m_screenFactorSet;
    static QDpi m_logicalDpi;
};

// Coordinate system conversion functions:
// QHighDpi::fromNativePixels   : from physical(screen/backing) to logical pixels
// QHighDpi::toNativePixels              : from logical to physical pixels

namespace QHighDpi {

inline qreal scale(qreal value, qreal scaleFactor, QPointF /* origin */ = QPointF(0, 0))
{
    return value * scaleFactor;
}

inline QSize scale(const QSize &value, qreal scaleFactor, QPointF /* origin */ = QPointF(0, 0))
{
    return value * scaleFactor;
}

inline QSizeF scale(const QSizeF &value, qreal scaleFactor, QPointF /* origin */ = QPointF(0, 0))
{
    return value * scaleFactor;
}

inline QVector2D scale(const QVector2D &value, qreal scaleFactor, QPointF /* origin */ = QPointF(0, 0))
{
    return value * float(scaleFactor);
}

inline QPointF scale(const QPointF &pos, qreal scaleFactor, QPointF origin = QPointF(0, 0))
{
     return (pos - origin) * scaleFactor + origin;
}

inline QPoint scale(const QPoint &pos, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
     return (pos - origin) * scaleFactor + origin;
}

inline QRect scale(const QRect &rect, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    return QRect(scale(rect.topLeft(), scaleFactor, origin), scale(rect.size(), scaleFactor));
}

inline QRectF scale(const QRectF &rect, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    return QRectF(scale(rect.topLeft(), scaleFactor, origin), scale(rect.size(), scaleFactor));
}

inline QMargins scale(const QMargins &margins, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    Q_UNUSED(origin)
    return QMargins(qRound(qreal(margins.left()) * scaleFactor), qRound(qreal(margins.top()) * scaleFactor),
                    qRound(qreal(margins.right()) * scaleFactor), qRound(qreal(margins.bottom()) * scaleFactor));
}

template <typename T>
QVector<T> scale(const QVector<T> &vector, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    if (!QHighDpiScaling::isActive())
        return vector;

    QVector<T> scaled;
    scaled.reserve(vector.size());
    for (const T &item : vector)
        scaled.append(scale(item, scaleFactor, origin));
    return scaled;
}

inline QRegion scale(const QRegion &region, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    if (!QHighDpiScaling::isActive())
        return region;

    QRegion scaled;
    for (const QRect &rect : region)
        scaled += scale(rect, scaleFactor, origin);
    return scaled;
}

template <typename T>
inline QPoint position(T) { return QPoint(); }
inline QPoint position(QPoint point) { return point; }
inline QPoint position(QPointF point) { return point.toPoint(); }
inline QPoint position(QRect rect) { return rect.center(); }
inline QPoint position(QRectF rect) { return rect.center().toPoint(); }

template <typename T, typename C>
T fromNativePixels(const T &value, const C *context)
{
    QPoint nativePosition = position(value);
    QHighDpiScaling::ScaleAndOrigin so = QHighDpiScaling::scaleAndOrigin(context, &nativePosition);
    return scale(value, qreal(1) / so.factor, so.origin);
}

template <typename T, typename C>
T toNativePixels(const T &value, const C *context)
{
    QHighDpiScaling::ScaleAndOrigin so = QHighDpiScaling::scaleAndOrigin(context);
    return scale(value, so.factor, so.origin);
}

template <typename T, typename C>
T fromNativeLocalPosition(const T &value, const C *context)
{
    return scale(value, qreal(1) / QHighDpiScaling::factor(context));
}

template <typename T, typename C>
T toNativeLocalPosition(const T &value, const C *context)
{
    return scale(value, QHighDpiScaling::factor(context));
}

template <typename T>
inline T fromNative(const T &value, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    return scale(value, qreal(1) / scaleFactor, origin);
}

template <typename T>
inline T toNative(const T &value, qreal scaleFactor, QPoint origin = QPoint(0, 0))
{
    return scale(value, scaleFactor, origin);
}

inline QRect fromNative(const QRect &rect, const QScreen *screen, const QPoint &screenOrigin)
{
    return scale(rect, qreal(1) / QHighDpiScaling::factor(screen), screenOrigin);
}

inline QRect fromNativeScreenGeometry(const QRect &nativeScreenGeometry, const QScreen *screen)
{
    return QRect(nativeScreenGeometry.topLeft(),
                 scale(nativeScreenGeometry.size(), qreal(1) / QHighDpiScaling::factor(screen)));
}

inline QRegion fromNativeLocalRegion(const QRegion &pixelRegion, const QWindow *window)
{
    return scale(pixelRegion, qreal(1) / QHighDpiScaling::factor(window));
}

// When mapping expose events to Qt rects: round top/left towards the origin and
// bottom/right away from the origin, making sure that we cover the whole window.
inline QRegion fromNativeLocalExposedRegion(const QRegion &pixelRegion, const QWindow *window)
{
    if (!QHighDpiScaling::isActive())
        return pixelRegion;

    const qreal scaleFactor = QHighDpiScaling::factor(window);
    QRegion pointRegion;
    for (const QRectF &rect : pixelRegion) {
        const QPointF topLeftP = rect.topLeft() / scaleFactor;
        const QSizeF sizeP = rect.size() / scaleFactor;
        pointRegion += QRect(QPoint(qFloor(topLeftP.x()), qFloor(topLeftP.y())),
                             QPoint(qCeil(topLeftP.x() + sizeP.width()  - 1.0),
                                    qCeil(topLeftP.y() + sizeP.height() - 1.0)));
    }
    return pointRegion;
}

inline QRegion toNativeLocalRegion(const QRegion &pointRegion, const QWindow *window)
{
    return scale(pointRegion, QHighDpiScaling::factor(window));
}

} // namespace QHighDpi
#else // QT_NO_HIGHDPISCALING
class Q_GUI_EXPORT QHighDpiScaling {
public:
    static inline void initHighDpiScaling() {}
    static inline void updateHighDpiScaling() {}
    static inline void setGlobalFactor(qreal) {}
    static inline void setScreenFactor(QScreen *, qreal) {}

    static inline bool isActive() { return false; }
    static inline qreal factor(const QWindow *) { return 1.0; }
    static inline qreal factor(const QScreen *) { return 1.0; }
    static inline qreal factor(const QPlatformScreen *) { return 1.0; }
    static inline QPoint origin(const QScreen *) { return QPoint(); }
    static inline QPoint origin(const QPlatformScreen *) { return QPoint(); }
    static inline QPoint mapPositionFromNative(const QPoint &pos, const QPlatformScreen *) { return pos; }
    static inline QPoint mapPositionToNative(const QPoint &pos, const QPlatformScreen *) { return pos; }
    static inline QPoint mapPositionToGlobal(const QPoint &pos, const QPoint &windowGlobalPosition, const QWindow *window) { return pos; }
    static inline QPoint mapPositionFromGlobal(const QPoint &pos, const QPoint &windowGlobalPosition, const QWindow *window) { return pos; }
    static inline QDpi logicalDpi() { return QDpi(-1,-1); }
};

namespace QHighDpi {
    template <typename T> inline
    T toNative(const T &value, ...) { return value; }
    template <typename T> inline
    T fromNative(const T &value, ...) { return value; }

    template <typename T> inline
    T fromNativeLocalPosition(const T &value, ...) { return value; }
    template <typename T> inline
    T toNativeLocalPosition(const T &value, ...) { return value; }

    template <typename T> inline
    T fromNativeLocalRegion(const T &value, ...) { return value; }
    template <typename T> inline
    T fromNativeLocalExposedRegion(const T &value, ...) { return value; }
    template <typename T> inline
    T toNativeLocalRegion(const T &value, ...) { return value; }

    template <typename T> inline
    T fromNativeScreenGeometry(const T &value, ...) { return value; }

    template <typename T, typename U> inline
    T toNativePixels(const T &value, const U*) {return value;}
    template <typename T, typename U> inline
    T fromNativePixels(const T &value, const U*) {return value;}
}
#endif // QT_NO_HIGHDPISCALING
QT_END_NAMESPACE

#endif // QHIGHDPISCALING_P_H
