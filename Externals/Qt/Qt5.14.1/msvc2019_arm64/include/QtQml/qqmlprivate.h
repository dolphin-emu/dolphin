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

#ifndef QQMLPRIVATE_H
#define QQMLPRIVATE_H

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

#include <functional>

#include <QtQml/qtqmlglobal.h>

#include <QtCore/qglobal.h>
#include <QtCore/qvariant.h>
#include <QtCore/qurl.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

namespace QQmlPrivate {
struct CachedQmlUnit;
}

namespace QV4 {
struct ExecutionEngine;
namespace CompiledData {
struct Unit;
struct CompilationUnit;
}
}
namespace QmlIR {
struct Document;
typedef void (*IRLoaderFunction)(Document *, const QQmlPrivate::CachedQmlUnit *);
}

typedef QObject *(*QQmlAttachedPropertiesFunc)(QObject *);

inline uint qHash(QQmlAttachedPropertiesFunc func, uint seed = 0)
{
    return qHash(quintptr(func), seed);
}

template <typename TYPE>
class QQmlTypeInfo
{
public:
    enum {
        hasAttachedProperties = 0
    };
};


class QJSValue;
class QJSEngine;
class QQmlEngine;
class QQmlCustomParser;
namespace QQmlPrivate
{
    void Q_QML_EXPORT qdeclarativeelement_destructor(QObject *);
    template<typename T>
    class QQmlElement final : public T
    {
    public:
        ~QQmlElement() override {
            QQmlPrivate::qdeclarativeelement_destructor(this);
        }
        static void operator delete(void *ptr) {
            // We allocate memory from this class in QQmlType::create
            // along with some additional memory.
            // So we override the operator delete in order to avoid the
            // sized operator delete to be called with a different size than
            // the size that was allocated.
            ::operator delete (ptr);
        }
        static void operator delete(void *, void *) {
            // Deliberately empty placement delete operator.
            // Silences MSVC warning C4291: no matching operator delete found
        }
    };

    template<typename T>
    void createInto(void *memory) { new (memory) QQmlElement<T>; }

    template<typename T>
    QObject *createParent(QObject *p) { return new T(p); }

    template<class From, class To, int N>
    struct StaticCastSelectorClass
    {
        static inline int cast() { return -1; }
    };

    template<class From, class To>
    struct StaticCastSelectorClass<From, To, sizeof(int)>
    {
        static inline int cast() { return int(reinterpret_cast<quintptr>(static_cast<To *>(reinterpret_cast<From *>(0x10000000)))) - 0x10000000; }
    };

    template<class From, class To>
    struct StaticCastSelector
    {
        typedef int yes_type;
        typedef char no_type;

        static yes_type checkType(To *);
        static no_type checkType(...);

        static inline int cast()
        {
            return StaticCastSelectorClass<From, To, sizeof(checkType(reinterpret_cast<From *>(0)))>::cast();
        }
    };

    template <typename T>
    struct has_attachedPropertiesMember
    {
        static bool const value = QQmlTypeInfo<T>::hasAttachedProperties;
    };

    template <typename T, bool hasMember>
    class has_attachedPropertiesMethod
    {
    public:
        typedef int yes_type;
        typedef char no_type;

        template<typename ReturnType>
        static yes_type checkType(ReturnType *(*)(QObject *));
        static no_type checkType(...);

        static bool const value = sizeof(checkType(&T::qmlAttachedProperties)) == sizeof(yes_type);
    };

    template <typename T>
    class has_attachedPropertiesMethod<T, false>
    {
    public:
        static bool const value = false;
    };

    template<typename T, int N>
    class AttachedPropertySelector
    {
    public:
        static inline QQmlAttachedPropertiesFunc func() { return nullptr; }
        static inline const QMetaObject *metaObject() { return nullptr; }
    };
    template<typename T>
    class AttachedPropertySelector<T, 1>
    {
        template<typename ReturnType>
        static inline const QMetaObject *attachedPropertiesMetaObject(ReturnType *(*)(QObject *)) {
            return &ReturnType::staticMetaObject;
        }
    public:
        static inline QQmlAttachedPropertiesFunc func() {
            return QQmlAttachedPropertiesFunc(&T::qmlAttachedProperties);
        }
        static inline const QMetaObject *metaObject() {
            return attachedPropertiesMetaObject(&T::qmlAttachedProperties);
        }
    };

    template<typename T>
    inline QQmlAttachedPropertiesFunc attachedPropertiesFunc()
    {
        return AttachedPropertySelector<T, has_attachedPropertiesMethod<T, has_attachedPropertiesMember<T>::value>::value>::func();
    }

    template<typename T>
    inline const QMetaObject *attachedPropertiesMetaObject()
    {
        return AttachedPropertySelector<T, has_attachedPropertiesMethod<T, has_attachedPropertiesMember<T>::value>::value>::metaObject();
    }

    enum AutoParentResult { Parented, IncompatibleObject, IncompatibleParent };
    typedef AutoParentResult (*AutoParentFunction)(QObject *object, QObject *parent);

    struct RegisterType {
        int version;

        int typeId;
        int listId;
        int objectSize;
        void (*create)(void *);
        QString noCreationReason;

        const char *uri;
        int versionMajor;
        int versionMinor;
        const char *elementName;
        const QMetaObject *metaObject;

        QQmlAttachedPropertiesFunc attachedPropertiesFunction;
        const QMetaObject *attachedPropertiesMetaObject;

        int parserStatusCast;
        int valueSourceCast;
        int valueInterceptorCast;

        QObject *(*extensionObjectCreate)(QObject *);
        const QMetaObject *extensionMetaObject;

        QQmlCustomParser *customParser;
        int revision;
        // If this is extended ensure "version" is bumped!!!
    };

    struct RegisterInterface {
        int version;

        int typeId;
        int listId;

        const char *iid;
    };

    struct RegisterAutoParent {
        int version;

        AutoParentFunction function;
    };

    struct RegisterSingletonType {
        int version;

        const char *uri;
        int versionMajor;
        int versionMinor;
        const char *typeName;

        QJSValue (*scriptApi)(QQmlEngine *, QJSEngine *);
        QObject *(*qobjectApi)(QQmlEngine *, QJSEngine *);
        const QMetaObject *instanceMetaObject; // new in version 1
        int typeId; // new in version 2
        int revision; // new in version 2
        std::function<QObject*(QQmlEngine *, QJSEngine *)> generalizedQobjectApi; // new in version 3
        // If this is extended ensure "version" is bumped!!!
    };

    struct RegisterCompositeType {
        QUrl url;
        const char *uri;
        int versionMajor;
        int versionMinor;
        const char *typeName;
    };

    struct RegisterCompositeSingletonType {
        QUrl url;
        const char *uri;
        int versionMajor;
        int versionMinor;
        const char *typeName;
    };

    struct CachedQmlUnit {
        const QV4::CompiledData::Unit *qmlData;
        void *unused1;
        void *unused2;
    };

    typedef const CachedQmlUnit *(*QmlUnitCacheLookupFunction)(const QUrl &url);
    struct RegisterQmlUnitCacheHook {
        int version;
        QmlUnitCacheLookupFunction lookupCachedQmlUnit;
    };

    enum RegistrationType {
        TypeRegistration       = 0,
        InterfaceRegistration  = 1,
        AutoParentRegistration = 2,
        SingletonRegistration  = 3,
        CompositeRegistration  = 4,
        CompositeSingletonRegistration = 5,
        QmlUnitCacheHookRegistration = 6
    };

    int Q_QML_EXPORT qmlregister(RegistrationType, void *);
    void Q_QML_EXPORT qmlunregister(RegistrationType, quintptr);
    struct Q_QML_EXPORT RegisterSingletonFunctor
    {
        QObject *operator()(QQmlEngine *, QJSEngine *);

        QPointer<QObject> m_object;
        bool alreadyCalled = false;
    };
}

QT_END_NAMESPACE

#endif // QQMLPRIVATE_H
