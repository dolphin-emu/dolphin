/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QCORE_MAC_P_H
#define QCORE_MAC_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qglobal_p.h"

#ifndef __IMAGECAPTURE__
#  define __IMAGECAPTURE__
#endif

// --------------------------------------------------------------------------

#if defined(QT_BOOTSTRAPPED)
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#include <functional>
#endif

#include "qstring.h"
#include "qscopedpointer.h"

#if defined( __OBJC__) && defined(QT_NAMESPACE)
#define QT_NAMESPACE_ALIAS_OBJC_CLASS(__KLASS__) @compatibility_alias __KLASS__ QT_MANGLE_NAMESPACE(__KLASS__)
#else
#define QT_NAMESPACE_ALIAS_OBJC_CLASS(__KLASS__)
#endif

#define QT_MAC_WEAK_IMPORT(symbol) extern "C" decltype(symbol) symbol __attribute__((weak_import));

QT_BEGIN_NAMESPACE
template <typename T, typename U, U (*RetainFunction)(U), void (*ReleaseFunction)(U)>
class QAppleRefCounted
{
public:
    QAppleRefCounted() : value() {}
    QAppleRefCounted(const T &t) : value(t) {}
    QAppleRefCounted(T &&t) noexcept(std::is_nothrow_move_constructible<T>::value)
        : value(std::move(t)) {}
    QAppleRefCounted(QAppleRefCounted &&other)
            noexcept(std::is_nothrow_move_assignable<T>::value &&
                     std::is_nothrow_move_constructible<T>::value)
        : value(qExchange(other.value, T())) {}
    QAppleRefCounted(const QAppleRefCounted &other) : value(other.value) { if (value) RetainFunction(value); }
    ~QAppleRefCounted() { if (value) ReleaseFunction(value); }
    operator T() const { return value; }
    void swap(QAppleRefCounted &other) noexcept(noexcept(qSwap(value, other.value)))
    { qSwap(value, other.value); }
    QAppleRefCounted &operator=(const QAppleRefCounted &other)
    { QAppleRefCounted copy(other); swap(copy); return *this; }
    QAppleRefCounted &operator=(QAppleRefCounted &&other)
        noexcept(std::is_nothrow_move_assignable<T>::value &&
                 std::is_nothrow_move_constructible<T>::value)
    { QAppleRefCounted moved(std::move(other)); swap(moved); return *this; }
    T *operator&() { return &value; }
protected:
    T value;
};


#ifdef Q_OS_MACOS
class QMacRootLevelAutoReleasePool
{
public:
    QMacRootLevelAutoReleasePool();
    ~QMacRootLevelAutoReleasePool();
private:
    QScopedPointer<QMacAutoReleasePool> pool;
};
#endif

/*
    Helper class that automates refernce counting for CFtypes.
    After constructing the QCFType object, it can be copied like a
    value-based type.

    Note that you must own the object you are wrapping.
    This is typically the case if you get the object from a Core
    Foundation function with the word "Create" or "Copy" in it. If
    you got the object from a "Get" function, either retain it or use
    constructFromGet(). One exception to this rule is the
    HIThemeGet*Shape functions, which in reality are "Copy" functions.
*/
template <typename T>
class QCFType : public QAppleRefCounted<T, CFTypeRef, CFRetain, CFRelease>
{
    using Base = QAppleRefCounted<T, CFTypeRef, CFRetain, CFRelease>;
public:
    using Base::Base;
    explicit QCFType(CFTypeRef r) : Base(static_cast<T>(r)) {}
    template <typename X> X as() const { return reinterpret_cast<X>(this->value); }
    static QCFType constructFromGet(const T &t)
    {
        if (t)
            CFRetain(t);
        return QCFType<T>(t);
    }
};

class Q_CORE_EXPORT QCFString : public QCFType<CFStringRef>
{
public:
    using QCFType<CFStringRef>::QCFType;
    inline QCFString(const QString &str) : QCFType<CFStringRef>(0), string(str) {}
    inline QCFString(const CFStringRef cfstr = 0) : QCFType<CFStringRef>(cfstr) {}
    inline QCFString(const QCFType<CFStringRef> &other) : QCFType<CFStringRef>(other) {}
    operator QString() const;
    operator CFStringRef() const;

private:
    QString string;
};

#ifdef Q_OS_MACOS
Q_CORE_EXPORT QChar qt_mac_qtKey2CocoaKey(Qt::Key key);
Q_CORE_EXPORT Qt::Key qt_mac_cocoaKey2QtKey(QChar keyCode);
Q_CORE_EXPORT bool qt_mac_applicationIsInDarkMode();
#endif

#ifndef QT_NO_DEBUG_STREAM
Q_CORE_EXPORT QDebug operator<<(QDebug debug, const QMacAutoReleasePool *pool);
Q_CORE_EXPORT QDebug operator<<(QDebug debug, const QCFString &string);
#endif

Q_CORE_EXPORT bool qt_apple_isApplicationExtension();

#if defined(Q_OS_MACOS) && !defined(QT_BOOTSTRAPPED)
Q_CORE_EXPORT bool qt_apple_isSandboxed();
# ifdef __OBJC__
QT_END_NAMESPACE
@interface NSObject (QtSandboxHelpers)
- (id)qt_valueForPrivateKey:(NSString *)key;
@end
QT_BEGIN_NAMESPACE
# endif
#endif

#if !defined(QT_BOOTSTRAPPED) && !defined(Q_OS_WATCHOS)
QT_END_NAMESPACE
# if defined(Q_OS_MACOS)
Q_FORWARD_DECLARE_OBJC_CLASS(NSApplication);
using AppleApplication = NSApplication;
# else
Q_FORWARD_DECLARE_OBJC_CLASS(UIApplication);
using AppleApplication = UIApplication;
# endif
QT_BEGIN_NAMESPACE
Q_CORE_EXPORT AppleApplication *qt_apple_sharedApplication();
#endif

// --------------------------------------------------------------------------

#if !defined(QT_BOOTSTRAPPED)
#define QT_USE_APPLE_UNIFIED_LOGGING

QT_END_NAMESPACE
#include <os/log.h>
QT_BEGIN_NAMESPACE

class Q_CORE_EXPORT AppleUnifiedLogger
{
public:
    static bool messageHandler(QtMsgType msgType, const QMessageLogContext &context, const QString &message,
        const QString &subsystem = QString());
    static bool willMirrorToStderr();
private:
    static os_log_type_t logTypeForMessageType(QtMsgType msgType);
    static os_log_t cachedLog(const QString &subsystem, const QString &category);
};

#endif

// --------------------------------------------------------------------------

#if !defined(QT_BOOTSTRAPPED)

QT_END_NAMESPACE
#include <os/activity.h>
QT_BEGIN_NAMESPACE

template <typename T> using QAppleOsType = QAppleRefCounted<T, void *, os_retain, os_release>;

class Q_CORE_EXPORT QAppleLogActivity
{
public:
    QAppleLogActivity() : activity(nullptr) {}
    QAppleLogActivity(os_activity_t activity) : activity(activity) {}
    ~QAppleLogActivity() { if (activity) leave(); }

    QAppleLogActivity(const QAppleLogActivity &) = delete;
    QAppleLogActivity& operator=(const QAppleLogActivity &) = delete;

    QAppleLogActivity(QAppleLogActivity&& other)
        : activity(other.activity), state(other.state) { other.activity = nullptr; }

    QAppleLogActivity& operator=(QAppleLogActivity &&other)
    {
        if (this != &other) {
            activity = other.activity;
            state = other.state;
            other.activity = nullptr;
        }
        return *this;
    }

    QAppleLogActivity&& enter()
    {
        if (activity)
            os_activity_scope_enter(static_cast<os_activity_t>(*this), &state);
        return std::move(*this);
    }

    void leave() {
        if (activity)
            os_activity_scope_leave(&state);
    }

    operator os_activity_t()
    {
        return reinterpret_cast<os_activity_t>(static_cast<void *>(activity));
    }

private:
    // Work around API_AVAILABLE not working for templates by using void*
    QAppleOsType<void *> activity;
    os_activity_scope_state_s state;
};

#define QT_APPLE_LOG_ACTIVITY_CREATE(condition, description, parent) []() { \
        if (!(condition)) \
            return QAppleLogActivity(); \
        return QAppleLogActivity(os_activity_create(description, parent, OS_ACTIVITY_FLAG_DEFAULT)); \
    }()

#define QT_VA_ARGS_CHOOSE(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define QT_VA_ARGS_COUNT(...) QT_VA_ARGS_CHOOSE(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define QT_OVERLOADED_MACRO(MACRO, ...) _QT_OVERLOADED_MACRO(MACRO, QT_VA_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define _QT_OVERLOADED_MACRO(MACRO, ARGC) _QT_OVERLOADED_MACRO_EXPAND(MACRO, ARGC)
#define _QT_OVERLOADED_MACRO_EXPAND(MACRO, ARGC) MACRO##ARGC

#define QT_APPLE_LOG_ACTIVITY_WITH_PARENT3(condition, description, parent) QT_APPLE_LOG_ACTIVITY_CREATE(condition, description, parent)
#define QT_APPLE_LOG_ACTIVITY_WITH_PARENT2(description, parent) QT_APPLE_LOG_ACTIVITY_WITH_PARENT3(true, description, parent)
#define QT_APPLE_LOG_ACTIVITY_WITH_PARENT(...) QT_OVERLOADED_MACRO(QT_APPLE_LOG_ACTIVITY_WITH_PARENT, __VA_ARGS__)

QT_MAC_WEAK_IMPORT(_os_activity_current);
#define QT_APPLE_LOG_ACTIVITY2(condition, description) QT_APPLE_LOG_ACTIVITY_CREATE(condition, description, OS_ACTIVITY_CURRENT)
#define QT_APPLE_LOG_ACTIVITY1(description) QT_APPLE_LOG_ACTIVITY2(true, description)
#define QT_APPLE_LOG_ACTIVITY(...) QT_OVERLOADED_MACRO(QT_APPLE_LOG_ACTIVITY, __VA_ARGS__)

#define QT_APPLE_SCOPED_LOG_ACTIVITY(...) QAppleLogActivity scopedLogActivity = QT_APPLE_LOG_ACTIVITY(__VA_ARGS__).enter();

#endif // !defined(QT_BOOTSTRAPPED)

// -------------------------------------------------------------------------

#if defined( __OBJC__)
class QMacNotificationObserver
{
public:
    QMacNotificationObserver() {}

    template<typename Functor>
    QMacNotificationObserver(id object, NSNotificationName name, Functor callback) {
        observer = [[NSNotificationCenter defaultCenter] addObserverForName:name
            object:object queue:nil usingBlock:^(NSNotification *) {
                callback();
            }
        ];
    }

    QMacNotificationObserver(const QMacNotificationObserver& other) = delete;
    QMacNotificationObserver(QMacNotificationObserver&& other) : observer(other.observer) {
        other.observer = nil;
    }

    QMacNotificationObserver &operator=(const QMacNotificationObserver& other) = delete;
    QMacNotificationObserver &operator=(QMacNotificationObserver&& other) {
        if (this != &other) {
            remove();
            observer = other.observer;
            other.observer = nil;
        }
        return *this;
    }

    void remove() {
        if (observer)
            [[NSNotificationCenter defaultCenter] removeObserver:observer];
        observer = nil;
    }
    ~QMacNotificationObserver() { remove(); }

private:
    id observer = nil;
};

QT_END_NAMESPACE
@interface QT_MANGLE_NAMESPACE(KeyValueObserver) : NSObject
@end
QT_NAMESPACE_ALIAS_OBJC_CLASS(KeyValueObserver);
QT_BEGIN_NAMESPACE

class Q_CORE_EXPORT QMacKeyValueObserver
{
public:
    using Callback = std::function<void()>;

    QMacKeyValueObserver() {}

    // Note: QMacKeyValueObserver must not outlive the object observed!
    QMacKeyValueObserver(id object, NSString *keyPath, Callback callback,
        NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew)
        : object(object), keyPath(keyPath), callback(new Callback(callback))
    {
        addObserver(options);
    }

    QMacKeyValueObserver(const QMacKeyValueObserver &other)
         : QMacKeyValueObserver(other.object, other.keyPath, *other.callback.get()) {}

    QMacKeyValueObserver(QMacKeyValueObserver &&other) { swap(other, *this); }

    ~QMacKeyValueObserver() { removeObserver(); }

    QMacKeyValueObserver &operator=(const QMacKeyValueObserver &other) {
        QMacKeyValueObserver tmp(other);
        swap(tmp, *this);
        return *this;
    }

    QMacKeyValueObserver &operator=(QMacKeyValueObserver &&other) {
        QMacKeyValueObserver tmp(std::move(other));
        swap(tmp, *this);
        return *this;
    }

    void removeObserver();

private:
    void swap(QMacKeyValueObserver &first, QMacKeyValueObserver &second) {
        std::swap(first.object, second.object);
        std::swap(first.keyPath, second.keyPath);
        std::swap(first.callback, second.callback);
    }

    void addObserver(NSKeyValueObservingOptions options);

    id object = nil;
    NSString *keyPath = nullptr;
    std::unique_ptr<Callback> callback;

    static KeyValueObserver *observer;
};
#endif

// -------------------------------------------------------------------------

QT_END_NAMESPACE

#endif // QCORE_MAC_P_H
