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
#ifndef QWINDOWSYSTEMINTERFACE_P_H
#define QWINDOWSYSTEMINTERFACE_P_H

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
#include "qwindowsysteminterface.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QMutex>
#include <QList>
#include <QWaitCondition>
#include <QAtomicInt>

QT_BEGIN_NAMESPACE

class QWindowSystemEventHandler;

class Q_GUI_EXPORT QWindowSystemInterfacePrivate {
public:
    enum EventType {
        UserInputEvent = 0x100,
        Close = UserInputEvent | 0x01,
        GeometryChange = 0x02,
        Enter = UserInputEvent | 0x03,
        Leave = UserInputEvent | 0x04,
        ActivatedWindow = 0x05,
        WindowStateChanged = 0x06,
        Mouse = UserInputEvent | 0x07,
        FrameStrutMouse = UserInputEvent | 0x08,  // ### Qt6 remove
        Wheel = UserInputEvent | 0x09,
        Key = UserInputEvent | 0x0a,
        Touch = UserInputEvent | 0x0b,
        ScreenOrientation = 0x0c,
        ScreenGeometry = 0x0d,
        ScreenAvailableGeometry = 0x0e,
        ScreenLogicalDotsPerInch = 0x0f,
        ScreenRefreshRate = 0x10,
        ThemeChange = 0x11,
        Expose = 0x12,
        FileOpen = UserInputEvent | 0x13,
        Tablet = UserInputEvent | 0x14,
        TabletEnterProximity = UserInputEvent | 0x15,
        TabletLeaveProximity = UserInputEvent | 0x16,
        PlatformPanel = UserInputEvent | 0x17,
        ContextMenu = UserInputEvent | 0x18,
        EnterWhatsThisMode = UserInputEvent | 0x19,
#ifndef QT_NO_GESTURES
        Gesture = UserInputEvent | 0x1a,
#endif
        ApplicationStateChanged = 0x19,
        FlushEvents = 0x20,
        WindowScreenChanged = 0x21,
        SafeAreaMarginsChanged = 0x22,
        ApplicationTermination = 0x23
    };

    class WindowSystemEvent {
    public:
        enum {
            Synthetic = 0x1,
            NullWindow = 0x2
        };

        explicit WindowSystemEvent(EventType t)
            : type(t), flags(0), eventAccepted(true) { }
        virtual ~WindowSystemEvent() { }

        bool synthetic() const  { return flags & Synthetic; }
        bool nullWindow() const { return flags & NullWindow; }

        EventType type;
        int flags;
        bool eventAccepted;
    };

    class CloseEvent : public WindowSystemEvent {
    public:
        explicit CloseEvent(QWindow *w)
            : WindowSystemEvent(Close), window(w)
            { }
        QPointer<QWindow> window;
    };

    class GeometryChangeEvent : public WindowSystemEvent {
    public:
        GeometryChangeEvent(QWindow *window, const QRect &newGeometry);
        QPointer<QWindow> window;
        QRect requestedGeometry;
        QRect newGeometry;
    };

    class EnterEvent : public WindowSystemEvent {
    public:
        explicit EnterEvent(QWindow *enter, const QPointF &local, const QPointF &global)
            : WindowSystemEvent(Enter), enter(enter), localPos(local), globalPos(global)
        { }
        QPointer<QWindow> enter;
        const QPointF localPos;
        const QPointF globalPos;
    };

    class LeaveEvent : public WindowSystemEvent {
    public:
        explicit LeaveEvent(QWindow *leave)
            : WindowSystemEvent(Leave), leave(leave)
        { }
        QPointer<QWindow> leave;
    };

    class ActivatedWindowEvent : public WindowSystemEvent {
    public:
        explicit ActivatedWindowEvent(QWindow *activatedWindow, Qt::FocusReason r)
            : WindowSystemEvent(ActivatedWindow), activated(activatedWindow), reason(r)
        { }
        QPointer<QWindow> activated;
        Qt::FocusReason reason;
    };

    class WindowStateChangedEvent : public WindowSystemEvent {
    public:
        WindowStateChangedEvent(QWindow *_window, Qt::WindowStates _newState, Qt::WindowStates _oldState)
            : WindowSystemEvent(WindowStateChanged), window(_window), newState(_newState), oldState(_oldState)
        { }

        QPointer<QWindow> window;
        Qt::WindowStates newState;
        Qt::WindowStates oldState;
    };

    class WindowScreenChangedEvent : public WindowSystemEvent {
    public:
        WindowScreenChangedEvent(QWindow *w, QScreen *s)
            : WindowSystemEvent(WindowScreenChanged), window(w), screen(s)
        { }

        QPointer<QWindow> window;
        QPointer<QScreen> screen;
    };

    class SafeAreaMarginsChangedEvent : public WindowSystemEvent {
    public:
        SafeAreaMarginsChangedEvent(QWindow *w)
            : WindowSystemEvent(SafeAreaMarginsChanged), window(w)
        { }

        QPointer<QWindow> window;
    };

    class ApplicationStateChangedEvent : public WindowSystemEvent {
    public:
        ApplicationStateChangedEvent(Qt::ApplicationState newState, bool forcePropagate = false)
            : WindowSystemEvent(ApplicationStateChanged), newState(newState), forcePropagate(forcePropagate)
        { }

        Qt::ApplicationState newState;
        bool forcePropagate;
    };

    class FlushEventsEvent : public WindowSystemEvent {
    public:
        FlushEventsEvent(QEventLoop::ProcessEventsFlags f = QEventLoop::AllEvents)
            : WindowSystemEvent(FlushEvents)
            , flags(f)
        { }
        QEventLoop::ProcessEventsFlags flags;
    };

    class UserEvent : public WindowSystemEvent {
    public:
        UserEvent(QWindow * w, ulong time, EventType t)
            : WindowSystemEvent(t), window(w), timestamp(time)
        {
            if (!w)
                flags |= NullWindow;
        }
        QPointer<QWindow> window;
        unsigned long timestamp;
    };

    class InputEvent: public UserEvent {
    public:
        InputEvent(QWindow * w, ulong time, EventType t, Qt::KeyboardModifiers mods)
            : UserEvent(w, time, t), modifiers(mods) {}
        Qt::KeyboardModifiers modifiers;
    };

    class MouseEvent : public InputEvent {
    public:
        MouseEvent(QWindow *w, ulong time, const QPointF &local, const QPointF &global,
                   Qt::MouseButtons state, Qt::KeyboardModifiers mods,
                   Qt::MouseButton b, QEvent::Type type,
                   Qt::MouseEventSource src = Qt::MouseEventNotSynthesized, bool frame = false)
            : InputEvent(w, time, Mouse, mods), localPos(local), globalPos(global), buttons(state),
              source(src), nonClientArea(frame), button(b), buttonType(type) { }

        // ### In Qt6 this method can be removed as there won't be need for compatibility code path
        bool enhancedMouseEvent() const
        {
            static const bool disableEnhanced = qEnvironmentVariableIsSet("QT_QPA_DISABLE_ENHANCED_MOUSE");
            return !disableEnhanced && buttonType != QEvent::None;
        }

        QPointF localPos;
        QPointF globalPos;
        Qt::MouseButtons buttons;
        Qt::MouseEventSource source;
        bool nonClientArea;
        Qt::MouseButton button;
        QEvent::Type buttonType;
    };

    class WheelEvent : public InputEvent {
    public:
        WheelEvent(QWindow *w, ulong time, const QPointF &local, const QPointF &global, QPoint pixelD, QPoint angleD, int qt4D, Qt::Orientation qt4O,
                   Qt::KeyboardModifiers mods, Qt::ScrollPhase phase = Qt::NoScrollPhase, Qt::MouseEventSource src = Qt::MouseEventNotSynthesized, bool inverted = false);
        QPoint pixelDelta;
        QPoint angleDelta;
        int qt4Delta;
        Qt::Orientation qt4Orientation;
        QPointF localPos;
        QPointF globalPos;
        Qt::ScrollPhase phase;
        Qt::MouseEventSource source;
        bool inverted;
    };

    class KeyEvent : public InputEvent {
    public:
        KeyEvent(QWindow *w, ulong time, QEvent::Type t, int k, Qt::KeyboardModifiers mods, const QString & text = QString(), bool autorep = false, ushort count = 1)
            :InputEvent(w, time, Key, mods), key(k), unicode(text), repeat(autorep),
             repeatCount(count), keyType(t),
             nativeScanCode(0), nativeVirtualKey(0), nativeModifiers(0) { }
        KeyEvent(QWindow *w, ulong time, QEvent::Type t, int k, Qt::KeyboardModifiers mods,
                 quint32 nativeSC, quint32 nativeVK, quint32 nativeMods,
                 const QString & text = QString(), bool autorep = false, ushort count = 1)
            :InputEvent(w, time, Key, mods), key(k), unicode(text), repeat(autorep),
             repeatCount(count), keyType(t),
             nativeScanCode(nativeSC), nativeVirtualKey(nativeVK), nativeModifiers(nativeMods) { }
        int key;
        QString unicode;
        bool repeat;
        ushort repeatCount;
        QEvent::Type keyType;
        quint32 nativeScanCode;
        quint32 nativeVirtualKey;
        quint32 nativeModifiers;
    };

    class TouchEvent : public InputEvent {
    public:
        TouchEvent(QWindow *w, ulong time, QEvent::Type t, QTouchDevice *dev,
                   const QList<QTouchEvent::TouchPoint> &p, Qt::KeyboardModifiers mods)
            :InputEvent(w, time, Touch, mods), device(dev), points(p), touchType(t) { }
        QTouchDevice *device;
        QList<QTouchEvent::TouchPoint> points;
        QEvent::Type touchType;
    };

    class ScreenOrientationEvent : public WindowSystemEvent {
    public:
        ScreenOrientationEvent(QScreen *s, Qt::ScreenOrientation o)
            : WindowSystemEvent(ScreenOrientation), screen(s), orientation(o) { }
        QPointer<QScreen> screen;
        Qt::ScreenOrientation orientation;
    };

    class ScreenGeometryEvent : public WindowSystemEvent {
    public:
        ScreenGeometryEvent(QScreen *s, const QRect &g, const QRect &ag)
            : WindowSystemEvent(ScreenGeometry), screen(s), geometry(g), availableGeometry(ag) { }
        QPointer<QScreen> screen;
        QRect geometry;
        QRect availableGeometry;
    };

    class ScreenLogicalDotsPerInchEvent : public WindowSystemEvent {
    public:
        ScreenLogicalDotsPerInchEvent(QScreen *s, qreal dx, qreal dy)
            : WindowSystemEvent(ScreenLogicalDotsPerInch), screen(s), dpiX(dx), dpiY(dy) { }
        QPointer<QScreen> screen;
        qreal dpiX;
        qreal dpiY;
    };

    class ScreenRefreshRateEvent : public WindowSystemEvent {
    public:
        ScreenRefreshRateEvent(QScreen *s, qreal r)
            : WindowSystemEvent(ScreenRefreshRate), screen(s), rate(r) { }
        QPointer<QScreen> screen;
        qreal rate;
    };

    class ThemeChangeEvent : public WindowSystemEvent {
    public:
        explicit ThemeChangeEvent(QWindow * w)
            : WindowSystemEvent(ThemeChange), window(w) { }
        QPointer<QWindow> window;
    };

    class ExposeEvent : public WindowSystemEvent {
    public:
        ExposeEvent(QWindow *window, const QRegion &region);
        QPointer<QWindow> window;
        bool isExposed;
        QRegion region;
    };

    class FileOpenEvent : public WindowSystemEvent {
    public:
        FileOpenEvent(const QString& fileName)
            : WindowSystemEvent(FileOpen), url(QUrl::fromLocalFile(fileName))
        { }
        FileOpenEvent(const QUrl &url)
            : WindowSystemEvent(FileOpen), url(url)
        { }
        QUrl url;
    };

    class Q_GUI_EXPORT TabletEvent : public InputEvent {
    public:
        static void handleTabletEvent(QWindow *w, const QPointF &local, const QPointF &global,
                                      int device, int pointerType, Qt::MouseButtons buttons, qreal pressure, int xTilt, int yTilt,
                                      qreal tangentialPressure, qreal rotation, int z, qint64 uid,
                                      Qt::KeyboardModifiers modifiers = Qt::NoModifier);
        static void setPlatformSynthesizesMouse(bool v);

        TabletEvent(QWindow *w, ulong time, const QPointF &local, const QPointF &global,
                    int device, int pointerType, Qt::MouseButtons b, qreal pressure, int xTilt, int yTilt, qreal tpressure,
                    qreal rotation, int z, qint64 uid, Qt::KeyboardModifiers mods)
            : InputEvent(w, time, Tablet, mods),
              buttons(b), local(local), global(global), device(device), pointerType(pointerType),
              pressure(pressure), xTilt(xTilt), yTilt(yTilt), tangentialPressure(tpressure),
              rotation(rotation), z(z), uid(uid) { }
        Qt::MouseButtons buttons;
        QPointF local;
        QPointF global;
        int device;
        int pointerType;
        qreal pressure;
        int xTilt;
        int yTilt;
        qreal tangentialPressure;
        qreal rotation;
        int z;
        qint64 uid;
        static bool platformSynthesizesMouse;
    };

    class TabletEnterProximityEvent : public InputEvent {
    public:
        TabletEnterProximityEvent(ulong time, int device, int pointerType, qint64 uid)
            : InputEvent(nullptr, time, TabletEnterProximity, Qt::NoModifier),
              device(device), pointerType(pointerType), uid(uid) { }
        int device;
        int pointerType;
        qint64 uid;
    };

    class TabletLeaveProximityEvent : public InputEvent {
    public:
        TabletLeaveProximityEvent(ulong time, int device, int pointerType, qint64 uid)
            : InputEvent(nullptr, time, TabletLeaveProximity, Qt::NoModifier),
              device(device), pointerType(pointerType), uid(uid) { }
        int device;
        int pointerType;
        qint64 uid;
    };

    class PlatformPanelEvent : public WindowSystemEvent {
    public:
        explicit PlatformPanelEvent(QWindow *w)
            : WindowSystemEvent(PlatformPanel), window(w) { }
        QPointer<QWindow> window;
    };

#ifndef QT_NO_CONTEXTMENU
    class ContextMenuEvent : public WindowSystemEvent {
    public:
        explicit ContextMenuEvent(QWindow *w, bool mouseTriggered, const QPoint &pos,
                                  const QPoint &globalPos, Qt::KeyboardModifiers modifiers)
            : WindowSystemEvent(ContextMenu), window(w), mouseTriggered(mouseTriggered), pos(pos),
              globalPos(globalPos), modifiers(modifiers) { }
        QPointer<QWindow> window;
        bool mouseTriggered;
        QPoint pos;       // Only valid if triggered by mouse
        QPoint globalPos; // Only valid if triggered by mouse
        Qt::KeyboardModifiers modifiers;
    };
#endif

#ifndef QT_NO_GESTURES
    class GestureEvent : public InputEvent {
    public:
        GestureEvent(QWindow *window, ulong time, Qt::NativeGestureType type, QTouchDevice *dev, QPointF pos, QPointF globalPos)
            : InputEvent(window, time, Gesture, Qt::NoModifier), type(type), pos(pos), globalPos(globalPos),
              realValue(0), sequenceId(0), intValue(0), device(dev) { }
        Qt::NativeGestureType type;
        QPointF pos;
        QPointF globalPos;
        // Mac
        qreal realValue;
        // Windows
        ulong sequenceId;
        quint64 intValue;
        QTouchDevice *device;
    };
#endif

    class WindowSystemEventList {
        QList<WindowSystemEvent *> impl;
        mutable QMutex mutex;
    public:
        WindowSystemEventList() : impl(), mutex() {}
        ~WindowSystemEventList() { clear(); }

        void clear()
        { const QMutexLocker locker(&mutex); qDeleteAll(impl); impl.clear(); }
        void prepend(WindowSystemEvent *e)
        { const QMutexLocker locker(&mutex); impl.prepend(e); }
        WindowSystemEvent *takeFirstOrReturnNull()
        { const QMutexLocker locker(&mutex); return impl.empty() ? 0 : impl.takeFirst(); }
        WindowSystemEvent *takeFirstNonUserInputOrReturnNull()
        {
            const QMutexLocker locker(&mutex);
            for (int i = 0; i < impl.size(); ++i)
                if (!(impl.at(i)->type & QWindowSystemInterfacePrivate::UserInputEvent))
                    return impl.takeAt(i);
            return nullptr;
        }
        bool nonUserInputEventsQueued()
        {
            const QMutexLocker locker(&mutex);
            for (int i = 0; i < impl.size(); ++i)
                if (!(impl.at(i)->type & QWindowSystemInterfacePrivate::UserInputEvent))
                    return true;
            return false;
        }
        void append(WindowSystemEvent *e)
        { const QMutexLocker locker(&mutex); impl.append(e); }
        int count() const
        { const QMutexLocker locker(&mutex); return impl.count(); }
        WindowSystemEvent *peekAtFirstOfType(EventType t) const
        {
            const QMutexLocker locker(&mutex);
            for (int i = 0; i < impl.size(); ++i) {
                if (impl.at(i)->type == t)
                    return impl.at(i);
            }
            return nullptr;
        }
        void remove(const WindowSystemEvent *e)
        {
            const QMutexLocker locker(&mutex);
            for (int i = 0; i < impl.size(); ++i) {
                if (impl.at(i) == e) {
                    delete impl.takeAt(i);
                    break;
                }
            }
        }
    private:
        Q_DISABLE_COPY_MOVE(WindowSystemEventList)
    };

    static WindowSystemEventList windowSystemEventQueue;

    static int windowSystemEventsQueued();
    static bool nonUserInputEventsQueued();
    static WindowSystemEvent *getWindowSystemEvent();
    static WindowSystemEvent *getNonUserInputWindowSystemEvent();
    static WindowSystemEvent *peekWindowSystemEvent(EventType t);
    static void removeWindowSystemEvent(WindowSystemEvent *event);
    template<typename Delivery = QWindowSystemInterface::DefaultDelivery>
    static bool handleWindowSystemEvent(WindowSystemEvent *ev);

public:
    static QElapsedTimer eventTime;
    static bool synchronousWindowSystemEvents;
    static bool platformFiltersEvents;

    static QWaitCondition eventsFlushed;
    static QMutex flushEventMutex;
    static QAtomicInt eventAccepted;

    static QList<QTouchEvent::TouchPoint>
        fromNativeTouchPoints(const QList<QWindowSystemInterface::TouchPoint> &points,
                              const QWindow *window, quint8 deviceId, QEvent::Type *type = nullptr);
    static QList<QWindowSystemInterface::TouchPoint>
        toNativeTouchPoints(const QList<QTouchEvent::TouchPoint>& pointList,
                            const QWindow *window);
    static void clearPointIdMap();

    static void installWindowSystemEventHandler(QWindowSystemEventHandler *handler);
    static void removeWindowSystemEventhandler(QWindowSystemEventHandler *handler);
    static QWindowSystemEventHandler *eventHandler;
};

class Q_GUI_EXPORT QWindowSystemEventHandler
{
public:
    virtual ~QWindowSystemEventHandler();
    virtual bool sendEvent(QWindowSystemInterfacePrivate::WindowSystemEvent *event);
};

QT_END_NAMESPACE

#endif // QWINDOWSYSTEMINTERFACE_P_H
