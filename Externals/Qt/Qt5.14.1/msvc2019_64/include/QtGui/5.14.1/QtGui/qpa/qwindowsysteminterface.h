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
#ifndef QWINDOWSYSTEMINTERFACE_H
#define QWINDOWSYSTEMINTERFACE_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtCore/QTime>
#include <QtGui/qwindowdefs.h>
#include <QtCore/QEvent>
#include <QtCore/QAbstractEventDispatcher>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtCore/QWeakPointer>
#include <QtCore/QMutex>
#include <QtGui/QTouchEvent>
#include <QtCore/QEventLoop>
#include <QtGui/QVector2D>

QT_BEGIN_NAMESPACE

class QMimeData;
class QTouchDevice;
class QPlatformDragQtResponse;
class QPlatformDropQtResponse;


class Q_GUI_EXPORT QWindowSystemInterface
{
public:
    struct SynchronousDelivery {};
    struct AsynchronousDelivery {};
    struct DefaultDelivery {};

#if QT_DEPRECATED_SINCE(5, 11)
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    QT_DEPRECATED static void handleMouseEvent(QWindow *window, const QPointF &local, const QPointF &global, Qt::MouseButtons b,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    QT_DEPRECATED static void handleMouseEvent(QWindow *window, ulong timestamp, const QPointF &local, const QPointF &global, Qt::MouseButtons b,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);

    QT_DEPRECATED static void handleFrameStrutMouseEvent(QWindow *window, const QPointF &local, const QPointF &global, Qt::MouseButtons b,
                                           Qt::KeyboardModifiers mods = Qt::NoModifier,
                                           Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);
    QT_DEPRECATED static void handleFrameStrutMouseEvent(QWindow *window, ulong timestamp, const QPointF &local, const QPointF &global, Qt::MouseButtons b,
                                           Qt::KeyboardModifiers mods = Qt::NoModifier,
                                           Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);
#endif
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleMouseEvent(QWindow *window, const QPointF &local, const QPointF &global,
                                 Qt::MouseButtons state, Qt::MouseButton button, QEvent::Type type,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleMouseEvent(QWindow *window, ulong timestamp, const QPointF &local,
                                 const QPointF &global, Qt::MouseButtons state,
                                 Qt::MouseButton button, QEvent::Type type,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);

    static void handleFrameStrutMouseEvent(QWindow *window, const QPointF &local,
                                           const QPointF &global, Qt::MouseButtons state,
                                           Qt::MouseButton button, QEvent::Type type,
                                           Qt::KeyboardModifiers mods = Qt::NoModifier,
                                           Qt::MouseEventSource source =
                                           Qt::MouseEventNotSynthesized);
    static void handleFrameStrutMouseEvent(QWindow *window, ulong timestamp, const QPointF &local,
                                           const QPointF &global, Qt::MouseButtons state,
                                           Qt::MouseButton button, QEvent::Type type,
                                           Qt::KeyboardModifiers mods = Qt::NoModifier,
                                           Qt::MouseEventSource source =
                                           Qt::MouseEventNotSynthesized);

    static bool handleShortcutEvent(QWindow *window, ulong timestamp, int k, Qt::KeyboardModifiers mods, quint32 nativeScanCode,
                                      quint32 nativeVirtualKey, quint32 nativeModifiers, const QString & text = QString(), bool autorep = false, ushort count = 1);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static bool handleKeyEvent(QWindow *window, QEvent::Type t, int k, Qt::KeyboardModifiers mods, const QString & text = QString(), bool autorep = false, ushort count = 1);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static bool handleKeyEvent(QWindow *window, ulong timestamp, QEvent::Type t, int k, Qt::KeyboardModifiers mods, const QString & text = QString(), bool autorep = false, ushort count = 1);

    static bool handleExtendedKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                                       quint32 nativeScanCode, quint32 nativeVirtualKey,
                                       quint32 nativeModifiers,
                                       const QString& text = QString(), bool autorep = false,
                                       ushort count = 1, bool tryShortcutOverride = true);
    static bool handleExtendedKeyEvent(QWindow *window, ulong timestamp, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                                       quint32 nativeScanCode, quint32 nativeVirtualKey,
                                       quint32 nativeModifiers,
                                       const QString& text = QString(), bool autorep = false,
                                       ushort count = 1, bool tryShortcutOverride = true);
    static void handleWheelEvent(QWindow *window, const QPointF &local, const QPointF &global,
                                 QPoint pixelDelta, QPoint angleDelta,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::ScrollPhase phase = Qt::NoScrollPhase,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized);
    static void handleWheelEvent(QWindow *window, ulong timestamp, const QPointF &local, const QPointF &global,
                                 QPoint pixelDelta, QPoint angleDelta,
                                 Qt::KeyboardModifiers mods = Qt::NoModifier,
                                 Qt::ScrollPhase phase = Qt::NoScrollPhase,
                                 Qt::MouseEventSource source = Qt::MouseEventNotSynthesized,
                                 bool inverted = false);

#if QT_DEPRECATED_SINCE(5, 10)
    QT_DEPRECATED static void handleWheelEvent(QWindow *window, const QPointF &local, const QPointF &global, int d, Qt::Orientation o, Qt::KeyboardModifiers mods = Qt::NoModifier);
    QT_DEPRECATED static void handleWheelEvent(QWindow *window, ulong timestamp, const QPointF &local, const QPointF &global, int d, Qt::Orientation o, Qt::KeyboardModifiers mods = Qt::NoModifier);
#endif

    struct TouchPoint {
        TouchPoint() : id(0), uniqueId(-1), pressure(0), rotation(0), state(Qt::TouchPointStationary) { }
        int id;                 // for application use
        qint64 uniqueId;        // for TUIO: object/token ID; otherwise empty
                                // TODO for TUIO 2.0: add registerPointerUniqueID(QPointingDeviceUniqueId)
        QPointF normalPosition; // touch device coordinates, (0 to 1, 0 to 1)
        QRectF area;            // dimensions of the elliptical contact patch, unrotated, and centered at position in screen coordinates
                                // width is the horizontal diameter, height is the vertical diameter
        qreal pressure;         // 0 to 1
        qreal rotation;         // rotation applied to the elliptical contact patch
                                // 0 means pointing straight up; 0 if unknown (like QTabletEvent::rotation)
        Qt::TouchPointState state; //Qt::TouchPoint{Pressed|Moved|Stationary|Released}
        QVector2D velocity;     // in screen coordinate system, pixels / seconds
        QTouchEvent::TouchPoint::InfoFlags flags;
        QVector<QPointF> rawPositions; // in screen coordinates
    };

    static void registerTouchDevice(const QTouchDevice *device);
    static void unregisterTouchDevice(const QTouchDevice *device);
    static bool isTouchDeviceRegistered(const QTouchDevice *device);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleTouchEvent(QWindow *window, QTouchDevice *device,
                                 const QList<struct TouchPoint> &points, Qt::KeyboardModifiers mods = Qt::NoModifier);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleTouchEvent(QWindow *window, ulong timestamp, QTouchDevice *device,
                                 const QList<struct TouchPoint> &points, Qt::KeyboardModifiers mods = Qt::NoModifier);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleTouchCancelEvent(QWindow *window, QTouchDevice *device, Qt::KeyboardModifiers mods = Qt::NoModifier);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleTouchCancelEvent(QWindow *window, ulong timestamp, QTouchDevice *device, Qt::KeyboardModifiers mods = Qt::NoModifier);

    // rect is relative to parent
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleGeometryChange(QWindow *window, const QRect &newRect);

    // region is in local coordinates, do not confuse with geometry which is parent-relative
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleExposeEvent(QWindow *window, const QRegion &region);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static bool handleCloseEvent(QWindow *window);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleEnterEvent(QWindow *window, const QPointF &local = QPointF(), const QPointF& global = QPointF());
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleLeaveEvent(QWindow *window);
    static void handleEnterLeaveEvent(QWindow *enter, QWindow *leave, const QPointF &local = QPointF(), const QPointF& global = QPointF());
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleWindowActivated(QWindow *window, Qt::FocusReason r = Qt::OtherFocusReason);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleWindowStateChanged(QWindow *window, Qt::WindowStates newState, int oldState = -1);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleWindowScreenChanged(QWindow *window, QScreen *newScreen);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleSafeAreaMarginsChanged(QWindow *window);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleApplicationStateChanged(Qt::ApplicationState newState, bool forcePropagate = false);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static bool handleApplicationTermination();

#if QT_CONFIG(draganddrop)
#if QT_DEPRECATED_SINCE(5, 11)
    QT_DEPRECATED static QPlatformDragQtResponse handleDrag(QWindow *window, const QMimeData *dropData,
                                              const QPoint &p, Qt::DropActions supportedActions);
    QT_DEPRECATED static QPlatformDropQtResponse handleDrop(QWindow *window, const QMimeData *dropData,
                                              const QPoint &p, Qt::DropActions supportedActions);
#endif // #if QT_DEPRECATED_SINCE(5, 11)
    static QPlatformDragQtResponse handleDrag(QWindow *window, const QMimeData *dropData,
                                              const QPoint &p, Qt::DropActions supportedActions,
                                              Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    static QPlatformDropQtResponse handleDrop(QWindow *window, const QMimeData *dropData,
                                              const QPoint &p, Qt::DropActions supportedActions,
                                              Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
#endif // QT_CONFIG(draganddrop)

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    static bool handleNativeEvent(QWindow *window, const QByteArray &eventType, void *message, qintptr *result);
#else
    static bool handleNativeEvent(QWindow *window, const QByteArray &eventType, void *message, long *result);
#endif

    // Changes to the screen
    static void handleScreenAdded(QPlatformScreen *screen, bool isPrimary = false);
    static void handleScreenRemoved(QPlatformScreen *screen);
    static void handlePrimaryScreenChanged(QPlatformScreen *newPrimary);

    static void handleScreenOrientationChange(QScreen *screen, Qt::ScreenOrientation newOrientation);
    static void handleScreenGeometryChange(QScreen *screen, const QRect &newGeometry, const QRect &newAvailableGeometry);
    static void handleScreenLogicalDotsPerInchChange(QScreen *screen, qreal newDpiX, qreal newDpiY);
    static void handleScreenRefreshRateChange(QScreen *screen, qreal newRefreshRate);

    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static void handleThemeChange(QWindow *window);

    static void handleFileOpenEvent(const QString& fileName);
    static void handleFileOpenEvent(const QUrl &url);

    static void handleTabletEvent(QWindow *window, ulong timestamp, const QPointF &local, const QPointF &global,
                                  int device, int pointerType, Qt::MouseButtons buttons, qreal pressure, int xTilt, int yTilt,
                                  qreal tangentialPressure, qreal rotation, int z, qint64 uid,
                                  Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    static void handleTabletEvent(QWindow *window, const QPointF &local, const QPointF &global,
                                  int device, int pointerType, Qt::MouseButtons buttons, qreal pressure, int xTilt, int yTilt,
                                  qreal tangentialPressure, qreal rotation, int z, qint64 uid,
                                  Qt::KeyboardModifiers modifiers = Qt::NoModifier);
#if QT_DEPRECATED_SINCE(5, 10)
    QT_DEPRECATED static void handleTabletEvent(QWindow *window, ulong timestamp, bool down, const QPointF &local, const QPointF &global,
                                                int device, int pointerType, qreal pressure, int xTilt, int yTilt,
                                                qreal tangentialPressure, qreal rotation, int z, qint64 uid,
                                                Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    QT_DEPRECATED static void handleTabletEvent(QWindow *window, bool down, const QPointF &local, const QPointF &global,
                                                int device, int pointerType, qreal pressure, int xTilt, int yTilt,
                                                qreal tangentialPressure, qreal rotation, int z, qint64 uid,
                                                Qt::KeyboardModifiers modifiers = Qt::NoModifier);
#endif
    static void handleTabletEnterProximityEvent(ulong timestamp, int device, int pointerType, qint64 uid);
    static void handleTabletEnterProximityEvent(int device, int pointerType, qint64 uid);
    static void handleTabletLeaveProximityEvent(ulong timestamp, int device, int pointerType, qint64 uid);
    static void handleTabletLeaveProximityEvent(int device, int pointerType, qint64 uid);

#ifndef QT_NO_GESTURES
    static void handleGestureEvent(QWindow *window, QTouchDevice *device,  ulong timestamp, Qt::NativeGestureType type,
                                   QPointF &local, QPointF &global);
    static void handleGestureEventWithRealValue(QWindow *window, QTouchDevice *device,  ulong timestamp, Qt::NativeGestureType type,
                                                qreal value, QPointF &local, QPointF &global);
    static void handleGestureEventWithSequenceIdAndValue(QWindow *window, QTouchDevice *device, ulong timestamp,Qt::NativeGestureType type,
                                                         ulong sequenceId, quint64 value, QPointF &local, QPointF &global);
#endif // QT_NO_GESTURES

    static void handlePlatformPanelEvent(QWindow *window);
#ifndef QT_NO_CONTEXTMENU
    static void handleContextMenuEvent(QWindow *window, bool mouseTriggered,
                                       const QPoint &pos, const QPoint &globalPos,
                                       Qt::KeyboardModifiers modifiers);
#endif
#if QT_CONFIG(whatsthis)
    static void handleEnterWhatsThisEvent();
#endif

    // For event dispatcher implementations
    static bool sendWindowSystemEvents(QEventLoop::ProcessEventsFlags flags);
    static void setSynchronousWindowSystemEvents(bool enable);
    static bool flushWindowSystemEvents(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents);
    static void deferredFlushWindowSystemEvents(QEventLoop::ProcessEventsFlags flags);
    static int windowSystemEventsQueued();
    static bool nonUserInputEventsQueued();
    static void setPlatformFiltersEvents(bool enable);
};

#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug dbg, const QWindowSystemInterface::TouchPoint &p);
#endif

QT_END_NAMESPACE

#endif // QWINDOWSYSTEMINTERFACE_H
