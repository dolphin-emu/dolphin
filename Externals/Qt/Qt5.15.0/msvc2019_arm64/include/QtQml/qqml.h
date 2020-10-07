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

#include <QtCore/qbytearray.h>
#include <QtCore/qmetaobject.h>

#define QML_VERSION     0x020000
#define QML_VERSION_STR "2.0"

#define QML_PRIVATE_NAMESPACE \
    QT_PREPEND_NAMESPACE(QQmlPrivate)

#define QML_REGISTER_TYPES_AND_REVISIONS \
    QT_PREPEND_NAMESPACE(qmlRegisterTypesAndRevisions)

#define QML_DECLARE_TYPE(TYPE) \
    Q_DECLARE_METATYPE(TYPE *) \
    Q_DECLARE_METATYPE(QQmlListProperty<TYPE>)

#define QML_DECLARE_TYPE_HASMETATYPE(TYPE) \
    Q_DECLARE_METATYPE(QQmlListProperty<TYPE>)

#define QML_DECLARE_INTERFACE(INTERFACE) \
    QML_DECLARE_TYPE(INTERFACE)

#define QML_DECLARE_INTERFACE_HASMETATYPE(INTERFACE) \
    QML_DECLARE_TYPE_HASMETATYPE(INTERFACE)

#define QML_ELEMENT \
    Q_CLASSINFO("QML.Element", "auto")

#define QML_ANONYMOUS \
    Q_CLASSINFO("QML.Element", "anonymous")

#define QML_NAMED_ELEMENT(NAME) \
    Q_CLASSINFO("QML.Element", #NAME)

#define QML_UNCREATABLE(REASON) \
    Q_CLASSINFO("QML.Creatable", "false") \
    Q_CLASSINFO("QML.UncreatableReason", REASON)

#define QML_SINGLETON \
    Q_CLASSINFO("QML.Singleton", "true") \
    enum class QmlIsSingleton {yes = true}; \
    template<typename, typename> friend struct QML_PRIVATE_NAMESPACE::QmlSingleton; \
    template<typename T, typename... Args> \
    friend void QML_REGISTER_TYPES_AND_REVISIONS(const char *uri, int versionMajor);

#define QML_ADDED_IN_MINOR_VERSION(VERSION) \
    Q_CLASSINFO("QML.AddedInMinorVersion", #VERSION)

#define QML_REMOVED_IN_MINOR_VERSION(VERSION) \
    Q_CLASSINFO("QML.RemovedInMinorVersion", #VERSION)

#define QML_ATTACHED(ATTACHED_TYPE) \
    Q_CLASSINFO("QML.Attached", #ATTACHED_TYPE) \
    using QmlAttachedType = ATTACHED_TYPE; \
    template<class, class, bool> friend struct QML_PRIVATE_NAMESPACE::QmlAttached; \
    template<class> friend struct QML_PRIVATE_NAMESPACE::QmlAttachedAccessor;

#define QML_EXTENDED(EXTENDED_TYPE) \
    Q_CLASSINFO("QML.Extended", #EXTENDED_TYPE) \
    using QmlExtendedType = EXTENDED_TYPE; \
    template<class, class> friend struct QML_PRIVATE_NAMESPACE::QmlExtended; \
    template<typename T, typename... Args> \
    friend void QML_REGISTER_TYPES_AND_REVISIONS(const char *uri, int versionMajor);

#define QML_FOREIGN(FOREIGN_TYPE) \
    Q_CLASSINFO("QML.Foreign", #FOREIGN_TYPE) \
    using QmlForeignType = FOREIGN_TYPE; \
    template<class, class> friend struct QML_PRIVATE_NAMESPACE::QmlResolved; \
    template<typename T, typename... Args> \
    friend void QML_REGISTER_TYPES_AND_REVISIONS(const char *uri, int versionMajor);

#define QML_INTERFACE \
    Q_CLASSINFO("QML.Element", "anonymous") \
    enum class QmlIsInterface {yes = true}; \
    template<typename, typename> friend struct QML_PRIVATE_NAMESPACE::QmlInterface; \
    template<typename T, typename... Args> \
    friend void QML_REGISTER_TYPES_AND_REVISIONS(const char *uri, int versionMajor);

#define QML_UNAVAILABLE \
    QML_FOREIGN(QQmlTypeNotAvailable)

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

void Q_QML_EXPORT qmlClearTypeRegistrations();

template<class T>
QQmlCustomParser *qmlCreateCustomParser();

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

int Q_QML_EXPORT qmlRegisterTypeNotAvailable(const char *uri, int versionMajor, int versionMinor,
                                             const char *qmlName, const QString& message);

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
int qmlRegisterExtendedType(const char *uri, int versionMajor)
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

        QQmlPrivate::createParent<E>, &E::staticMetaObject,

        nullptr,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

#if QT_DEPRECATED_SINCE(5, 15)
template<typename T, typename E>
QT_DEPRECATED_VERSION_X_5_15("Use qmlRegisterExtendedType(uri, versionMajor) instead")
int qmlRegisterExtendedType()
{
    return qmlRegisterExtendedType<T, E>("", 0);
}
#endif

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

#if QT_DEPRECATED_SINCE(5, 15)
template<typename T>
QT_DEPRECATED_VERSION_X_5_15("Use qmlRegisterInterface(uri, versionMajor) instead")
int qmlRegisterInterface(const char *typeName)
{
    QByteArray name(typeName);

    QByteArray pointerName(name + '*');
    QByteArray listName("QQmlListProperty<" + name + '>');

    QQmlPrivate::RegisterInterface qmlInterface = {
        1,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T> >(listName.constData()),

        qobject_interface_iid<T *>(),
        "",
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::InterfaceRegistration, &qmlInterface);
}
#endif

template<typename T>
int qmlRegisterInterface(const char *uri, int versionMajor)
{
    QML_GETTYPENAMES

    QQmlPrivate::RegisterInterface qmlInterface = {
        1,
        qRegisterNormalizedMetaType<T *>(pointerName.constData()),
        qRegisterNormalizedMetaType<QQmlListProperty<T>>(listName.constData()),
        qobject_interface_iid<T *>(),

        uri,
        versionMajor
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

#ifdef Q_QDOC
template <typename T>
int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName, std::function<QObject*(QQmlEngine *, QJSEngine *)> callback)
#else
template <typename T, typename F, typename std::enable_if<std::is_convertible<F, std::function<QObject *(QQmlEngine *, QJSEngine *)>>::value
                                                 && !std::is_convertible<F, QObject *(*)(QQmlEngine *, QJSEngine *)>::value, void>::type* = nullptr>
inline int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                    F&& callback)
#endif
{

    QML_GETTYPENAMES

    QQmlPrivate::RegisterSingletonType api = {
        QmlCurrentSingletonTypeRegistrationVersion,

        uri, versionMajor, versionMinor, typeName,

        nullptr, nullptr, &T::staticMetaObject, qRegisterNormalizedMetaType<T *>(pointerName.constData()), 0, callback
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::SingletonRegistration, &api);
}

#ifdef Q_QDOC
int qmlRegisterSingletonInstance(const char *uri, int versionMajor, int versionMinor, const char *typeName, QObject *cppObject)
#else
template<typename T>
inline auto qmlRegisterSingletonInstance(const char *uri, int versionMajor, int versionMinor,
                                         const char *typeName, T *cppObject) -> typename std::enable_if<std::is_base_of<QObject, T>::value, int>::type
#endif
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

template<class T, class Resolved, class Extended, bool Singleton, bool Interface>
struct QmlTypeAndRevisionsRegistration;

template<class T, class Resolved, class Extended>
struct QmlTypeAndRevisionsRegistration<T, Resolved, Extended, false, false> {
    static void registerTypeAndRevisions(const char *uri, int versionMajor)
    {
        QQmlPrivate::qmlRegisterTypeAndRevisions<Resolved, Extended>(
                    uri, versionMajor, &T::staticMetaObject);
    }
};

template<class T, class Resolved>
struct QmlTypeAndRevisionsRegistration<T, Resolved, void, true, false> {
    static void registerTypeAndRevisions(const char *uri, int versionMajor)
    {
        QQmlPrivate::qmlRegisterSingletonAndRevisions<Resolved>(
                    uri, versionMajor, &T::staticMetaObject);
    }
};

template<class T, class Resolved>
struct QmlTypeAndRevisionsRegistration<T, Resolved, void, false, true> {
    static void registerTypeAndRevisions(const char *uri, int versionMajor)
    {
        qmlRegisterInterface<Resolved>(uri, versionMajor);
    }
};

template<typename T = void, typename... Args>
void qmlRegisterTypesAndRevisions(const char *uri, int versionMajor);

template<typename T, typename... Args>
void qmlRegisterTypesAndRevisions(const char *uri, int versionMajor)
{
    QmlTypeAndRevisionsRegistration<
            T, typename QQmlPrivate::QmlResolved<T>::Type,
            typename QQmlPrivate::QmlExtended<T>::Type,
            QQmlPrivate::QmlSingleton<T>::Value,
            QQmlPrivate::QmlInterface<T>::Value>
            ::registerTypeAndRevisions(uri, versionMajor);
    qmlRegisterTypesAndRevisions<Args...>(uri, versionMajor);
}

template<>
inline void qmlRegisterTypesAndRevisions<>(const char *, int) {}

inline void qmlRegisterNamespaceAndRevisions(const QMetaObject *metaObject,
                                             const char *uri, int versionMajor)
{
    QQmlPrivate::RegisterTypeAndRevisions type = {
        0,
        0,
        0,
        0,
        nullptr,

        uri,
        versionMajor,

        metaObject,
        metaObject,

        nullptr,
        nullptr,

        -1,
        -1,
        -1,

        nullptr,
        nullptr,

        &qmlCreateCustomParser<void>
    };

    qmlregister(QQmlPrivate::TypeAndRevisionsRegistration, &type);
}

int Q_QML_EXPORT qmlTypeId(const char *uri, int versionMajor, int versionMinor, const char *qmlName);

QT_END_NAMESPACE

QML_DECLARE_TYPE(QObject)
Q_DECLARE_METATYPE(QVariant)

#endif // QQML_H
