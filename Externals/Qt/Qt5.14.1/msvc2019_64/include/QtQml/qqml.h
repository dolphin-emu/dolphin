/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQML_H
#define QQML_H

#include <QtQml/qqmlprivate.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmlpropertyvaluesource.h>
#include <QtQml/qqmllist.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qmetaobject.h>

#define QML_VERSION     0x020000
#define QML_VERSION_STR "2.0"

#define QML_DECLARE_TYPE(TYPE) \
    Q_DECLARE_METATYPE(TYPE *) \
    Q_DECLARE_METATYPE(QQmlListProperty<TYPE>)

#define QML_DECLARE_TYPE_HASMETATYPE(TYPE) \
    Q_DECLARE_METATYPE(QQmlListProperty<TYPE>)

#define QML_DECLARE_INTERFACE(INTERFACE) \
    QML_DECLARE_TYPE(INTERFACE)

#define QML_DECLARE_INTERFACE_HASMETATYPE(INTERFACE) \
    QML_DECLARE_TYPE_HASMETATYPE(INTERFACE)

enum { /* TYPEINFO flags */
    QML_HAS_ATTACHED_PROPERTIES = 0x01
};

#define QML_DECLARE_TYPEINFO(TYPE, FLAGS) \
QT_BEGIN_NAMESPACE \
template <> \
class QQmlTypeInfo<TYPE > \
{ \
public: \
    enum { \
        hasAttachedProperties = (((FLAGS) & QML_HAS_ATTACHED_PROPERTIES) == QML_HAS_ATTACHED_PROPERTIES) \
    }; \
}; \
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE


class QQmlPropertyValueInterceptor;

#define QML_GETTYPENAMES \
    const char *className = T::staticMetaObject.className(); \
    const int nameLen = int(strlen(className)); \
    QVarLengthArray<char,48> pointerName(nameLen+2); \
    memcpy(pointerName.data(), className, size_t(nameLen)); \
    pointerName[nameLen] = '*'; \
    pointerName[nameLen+1] = '\0'; \
    const int listLen = int(strlen("QQmlListProperty<")); \
    QVarLengthArray<char,64> listName(listLen + nameLen + 2); \
    memcpy(listName.data(), "QQmlListProperty<", size_t(listLen)); \
    memcpy(listName.data()+listLen, className, size_t(nameLen)); \
    listName[listLen+nameLen] = '>'; \
    listName[listLen+nameLen+1] = '\0';

void Q_QML_EXPORT qmlClearTypeRegistrations();

template<typename T>
int qmlRegisterAnonymousType(const char *uri, int versionMajor)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        QString(),

        uri, versionMajor, 0, nullptr, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

#if QT_DEPRECATED_SINCE(5, 14)
template<typename T>
QT_DEPRECATED_VERSION_X_5_14("Use qmlRegisterAnonymousType instead") int qmlRegisterType()
{
    return qmlRegisterAnonymousType<T>("", 1);
}
#endif

int Q_QML_EXPORT qmlRegisterTypeNotAvailable(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& message);

template<typename T>
int qmlRegisterUncreatableType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& reason)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        reason,

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, int metaObjectRevision>
int qmlRegisterUncreatableType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& reason)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        reason,

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        metaObjectRevision
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, typename E>
int qmlRegisterExtendedUncreatableType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& reason)
{
    QML_GETTYPENAMES

    QQmlAttachedPropertiesFunc attached = QQmlPrivate::attachedPropertiesFunc<E>();
    const QMetaObject * attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<E>();
    if (!attached) {
        attached = QQmlPrivate::attachedPropertiesFunc<T>();
        attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<T>();
    }

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        reason,

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        attached,
        attachedMetaObject,

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, typename E, int metaObjectRevision>
int qmlRegisterExtendedUncreatableType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& reason)
{
    QML_GETTYPENAMES

    QQmlAttachedPropertiesFunc attached = QQmlPrivate::attachedPropertiesFunc<E>();
    const QMetaObject * attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<E>();
    if (!attached) {
        attached = QQmlPrivate::attachedPropertiesFunc<T>();
        attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<T>();
    }

    QQmlPrivate::RegisterType type = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        reason,

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        attached,
        attachedMetaObject,

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        nullptr,
        metaObjectRevision
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

Q_QML_EXPORT int qmlRegisterUncreatableMetaObject(const QMetaObject &staticMetaObject, const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QString& reason);

template<typename T>
int qmlRegisterType(const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, int metaObjectRevision>
int qmlRegisterType(const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        metaObjectRevision
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, int metaObjectRevision>
int qmlRegisterRevision(const char *uri, int versionMajor, int versionMinor)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, nullptr, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        nullptr,
        metaObjectRevision
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}


template<typename T, typename E>
int qmlRegisterExtendedType()
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        0,
        nullptr,
        QString(),

        nullptr, 0, 0, nullptr, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, typename E>
int qmlRegisterExtendedType(const char *uri, int versionMajor, int versionMinor,
                            const char *qmlName)
{
    QML_GETTYPENAMES

    QQmlAttachedPropertiesFunc attached = QQmlPrivate::attachedPropertiesFunc<E>();
    const QMetaObject * attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<E>();
    if (!attached) {
        attached = QQmlPrivate::attachedPropertiesFunc<T>();
        attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<T>();
    }

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        attached,
        attachedMetaObject,

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T>
int qmlRegisterInterface(const char *typeName)
{
    QByteArray name(typeName);

    QByteArray pointerName(name + '*');
    QByteArray listName("QQmlListProperty<" + name + '>');

    QQmlPrivate::RegisterInterface qmlInterface = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),

        qobject_interface_iid<T *>()
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::InterfaceRegistration, &qmlInterface);
}

template<typename T>
int qmlRegisterCustomType(const char *uri, int versionMajor, int versionMinor,
                          const char *qmlName, QQmlCustomParser *parser)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        parser,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, int metaObjectRevision>
int qmlRegisterCustomType(const char *uri, int versionMajor, int versionMinor,
                          const char *qmlName, QQmlCustomParser *parser)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterType type = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        QQmlPrivate::attachedPropertiesFunc<T>(),
        QQmlPrivate::attachedPropertiesMetaObject<T>(),

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        nullptr, nullptr,

        parser,
        metaObjectRevision
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

template<typename T, typename E>
int qmlRegisterCustomExtendedType(const char *uri, int versionMajor, int versionMinor,
                          const char *qmlName, QQmlCustomParser *parser)
{
    QML_GETTYPENAMES

    QQmlAttachedPropertiesFunc attached = QQmlPrivate::attachedPropertiesFunc<E>();
    const QMetaObject * attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<E>();
    if (!attached) {
        attached = QQmlPrivate::attachedPropertiesFunc<T>();
        attachedMetaObject = QQmlPrivate::attachedPropertiesMetaObject<T>();
    }

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),
        sizeof(T), QQmlPrivate::createInto<T>,
        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        attached,
        attachedMetaObject,

        QQmlPrivate::StaticCastSelector<T,QQmlParserStatus>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueSource>::cast(),
        QQmlPrivate::StaticCastSelector<T,QQmlPropertyValueInterceptor>::cast(),

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        parser,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

class QQmlContext;
class QQmlEngine;
class QJSValue;
class QJSEngine;

#ifndef Q_QDOC
namespace QtQml {
#endif
    // declared in namespace to avoid symbol conflicts with QtDeclarative
    Q_QML_EXPORT void qmlExecuteDeferred(QObject *);
    Q_QML_EXPORT QQmlContext *qmlContext(const QObject *);
    Q_QML_EXPORT QQmlEngine *qmlEngine(const QObject *);
#if QT_DEPRECATED_SINCE(5, 14)
    Q_QML_EXPORT QT_DEPRECATED_VERSION_X_5_14("Use qmlAttachedPropertiesObject(QObject *, QQmlAttachedPropertiesFunc, bool")
    QObject *qmlAttachedPropertiesObjectById(int, const QObject *, bool create = true);
    Q_QML_EXPORT QT_DEPRECATED_VERSION_X_5_14("Use qmlAttachedPropertiesObject(QObject *, QQmlAttachedPropertiesFunc, bool")
    QObject *qmlAttachedPropertiesObject(
            int *, const QObject *, const QMetaObject *, bool create);
#endif
    Q_QML_EXPORT QQmlAttachedPropertiesFunc qmlAttachedPropertiesFunction(QObject *,
                                                                          const QMetaObject *);
    Q_QML_EXPORT QObject *qmlAttachedPropertiesObject(QObject *, QQmlAttachedPropertiesFunc func,
                                                      bool create = true);
#ifndef Q_QDOC
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wheader-hygiene")

// This is necessary to allow for QtQuick1 and QtQuick2 scenes in a single application.
using namespace QtQml;

QT_WARNING_POP

#endif // Q_QDOC

//The C++ version of protected namespaces in qmldir
Q_QML_EXPORT bool qmlProtectModule(const char* uri, int majVersion);
Q_QML_EXPORT void qmlRegisterModule(const char *uri, int versionMajor, int versionMinor);

template<typename T>
QObject *qmlAttachedPropertiesObject(const QObject *obj, bool create = true)
{
    // We don't need a concrete object to resolve the function. As T is a C++ type, it and all its
    // super types should be registered as CppType (or not at all). We only need the object and its
    // QML engine to resolve composite types. Therefore, the function is actually a static property
    // of the C++ type system and we can cache it here for improved performance on further lookups.
    static const auto func = qmlAttachedPropertiesFunction(nullptr, &T::staticMetaObject);
    return qmlAttachedPropertiesObject(const_cast<QObject *>(obj), func, create);
}

inline int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                QJSValue (*callback)(QQmlEngine *, QJSEngine *))
{
    QQmlPrivate::RegisterSingletonType api = {
        0,

        uri, versionMajor, versionMinor, typeName,

        callback, nullptr, nullptr, 0, 0, {}
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &api);
}

enum { QmlCurrentSingletonTypeRegistrationVersion = 3 };
template <typename T>
inline int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                QObject *(*callback)(QQmlEngine *, QJSEngine *))
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterSingletonType api = {
        QmlCurrentSingletonTypeRegistrationVersion,

        uri, versionMajor, versionMinor, typeName,

        nullptr, nullptr, &T::staticMetaObject, qRegisterNormalizedMetaType<T *>(pointerName.constData()), 0, callback
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &api);
}

template <typename T, typename F, typename std::enable_if<std::is_convertible<F, std::function<QObject *(QQmlEngine *, QJSEngine *)>>::value
                                                 && !std::is_convertible<F, QObject *(*)(QQmlEngine *, QJSEngine *)>::value, void>::type* = nullptr>
inline int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                    F&& callback)
{

    QML_GETTYPENAMES

    QQmlPrivate::RegisterSingletonType api = {
        QmlCurrentSingletonTypeRegistrationVersion,

        uri, versionMajor, versionMinor, typeName,

        nullptr, nullptr, &T::staticMetaObject, qRegisterNormalizedMetaType<T *>(pointerName.constData()), 0, callback
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &api);
}

template<typename T>
inline auto qmlRegisterSingletonInstance(const char *uri, int versionMajor, int versionMinor,
                                         const char *typeName, T *cppObject) -> typename std::enable_if<std::is_base_of<QObject, T>::value, int>::type
{
    QQmlPrivate::RegisterSingletonFunctor registrationFunctor;
    registrationFunctor.m_object = cppObject;
    return qmlRegisterSingletonType<T>(uri, versionMajor, versionMinor, typeName, registrationFunctor);
}

inline int qmlRegisterSingletonType(const QUrl &url, const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    if (url.isRelative()) {
        // User input check must go here, because QQmlPrivate::qmlregister is also used internally for composite types
        qWarning("qmlRegisterSingletonType requires absolute URLs.");
        return 0;
    }

    QQmlPrivate::RegisterCompositeSingletonType type = {
        url,
        uri,
        versionMajor,
        versionMinor,
        qmlName
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::CompositeSingletonRegistration, &type);
}

inline int qmlRegisterType(const QUrl &url, const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    if (url.isRelative()) {
        // User input check must go here, because QQmlPrivate::qmlregister is also used internally for composite types
        qWarning("qmlRegisterType requires absolute URLs.");
        return 0;
    }

    QQmlPrivate::RegisterCompositeType type = {
        url,
        uri,
        versionMajor,
        versionMinor,
        qmlName
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::CompositeRegistration, &type);
}

int Q_QML_EXPORT qmlTypeId(const char *uri, int versionMajor, int versionMinor, const char *qmlName);

QT_END_NAMESPACE

QML_DECLARE_TYPE(QObject)
Q_DECLARE_METATYPE(QVariant)

#endif // QQML_H
