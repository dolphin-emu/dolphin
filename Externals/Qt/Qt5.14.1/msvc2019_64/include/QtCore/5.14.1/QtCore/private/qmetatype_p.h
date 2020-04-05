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

#ifndef QMETATYPE_P_H
#define QMETATYPE_P_H

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

#include <QtCore/private/qglobal_p.h>
#include "qmetatype.h"

QT_BEGIN_NAMESPACE

namespace QModulesPrivate {
enum Names { Core, Gui, Widgets, Unknown, ModulesCount /* ModulesCount has to be at the end */ };

static inline int moduleForType(const uint typeId)
{
    if (typeId <= QMetaType::LastCoreType)
        return Core;
    if (typeId >= QMetaType::FirstGuiType && typeId <= QMetaType::LastGuiType)
        return Gui;
    if (typeId >= QMetaType::FirstWidgetsType && typeId <= QMetaType::LastWidgetsType)
        return Widgets;
    return Unknown;
}

template <typename T>
class QTypeModuleInfo
{
public:
    enum Module : bool {
        IsCore = false,
        IsWidget = false,
        IsGui = false,
        IsUnknown = true
    };
};

#define QT_ASSIGN_TYPE_TO_MODULE(TYPE, MODULE) \
template<> \
class QTypeModuleInfo<TYPE > \
{ \
public: \
    enum Module : bool { \
        IsCore = (((MODULE) == (QModulesPrivate::Core))), \
        IsWidget = (((MODULE) == (QModulesPrivate::Widgets))), \
        IsGui = (((MODULE) == (QModulesPrivate::Gui))), \
        IsUnknown = !(IsCore || IsWidget || IsGui) \
    }; \
    static inline int module() { return MODULE; } \
    Q_STATIC_ASSERT((IsUnknown && !(IsCore || IsWidget || IsGui)) \
                 || (IsCore && !(IsUnknown || IsWidget || IsGui)) \
                 || (IsWidget && !(IsUnknown || IsCore || IsGui)) \
                 || (IsGui && !(IsUnknown || IsCore || IsWidget))); \
};


#define QT_DECLARE_CORE_MODULE_TYPES_ITER(TypeName, TypeId, Name) \
    QT_ASSIGN_TYPE_TO_MODULE(Name, QModulesPrivate::Core);
#define QT_DECLARE_GUI_MODULE_TYPES_ITER(TypeName, TypeId, Name) \
    QT_ASSIGN_TYPE_TO_MODULE(Name, QModulesPrivate::Gui);
#define QT_DECLARE_WIDGETS_MODULE_TYPES_ITER(TypeName, TypeId, Name) \
    QT_ASSIGN_TYPE_TO_MODULE(Name, QModulesPrivate::Widgets);

QT_FOR_EACH_STATIC_PRIMITIVE_TYPE(QT_DECLARE_CORE_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_PRIMITIVE_POINTER(QT_DECLARE_CORE_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_CORE_CLASS(QT_DECLARE_CORE_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_CORE_POINTER(QT_DECLARE_CORE_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_CORE_TEMPLATE(QT_DECLARE_CORE_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_GUI_CLASS(QT_DECLARE_GUI_MODULE_TYPES_ITER)
QT_FOR_EACH_STATIC_WIDGETS_CLASS(QT_DECLARE_WIDGETS_MODULE_TYPES_ITER)
} // namespace QModulesPrivate

#undef QT_DECLARE_CORE_MODULE_TYPES_ITER
#undef QT_DECLARE_GUI_MODULE_TYPES_ITER
#undef QT_DECLARE_WIDGETS_MODULE_TYPES_ITER

class QMetaTypeInterface
{
public:
    QMetaType::SaveOperator saveOp;
    QMetaType::LoadOperator loadOp;
    QMetaType::Constructor constructor; // TODO Qt6: remove me
    QMetaType::Destructor destructor;
    int size;
    QMetaType::TypeFlags::Int flags;
    const QMetaObject *metaObject;
    QMetaType::TypedConstructor typedConstructor;
    QMetaType::TypedDestructor typedDestructor;
};

#ifndef QT_NO_DATASTREAM
#  define QT_METATYPE_INTERFACE_INIT_DATASTREAM_IMPL(Type) \
    /*saveOp*/(QtMetaTypePrivate::QMetaTypeFunctionHelper<Type, QtMetaTypePrivate::TypeDefinition<Type>::IsAvailable>::Save), \
    /*loadOp*/(QtMetaTypePrivate::QMetaTypeFunctionHelper<Type, QtMetaTypePrivate::TypeDefinition<Type>::IsAvailable>::Load),
#  define QT_METATYPE_INTERFACE_INIT_EMPTY_DATASTREAM_IMPL(Type) \
    /*saveOp*/ nullptr, \
    /*loadOp*/ nullptr,
#else
#  define QT_METATYPE_INTERFACE_INIT_EMPTY_DATASTREAM_IMPL(Type) \
    /*saveOp*/ nullptr, \
    /*loadOp*/ nullptr,
#  define QT_METATYPE_INTERFACE_INIT_DATASTREAM_IMPL(Type) \
    QT_METATYPE_INTERFACE_INIT_EMPTY_DATASTREAM_IMPL(Type)
#endif

#ifndef QT_BOOTSTRAPPED
#define METAOBJECT_DELEGATE(Type) (QtPrivate::MetaObjectForType<Type>::value())
#else
#define METAOBJECT_DELEGATE(Type) nullptr
#endif

#define QT_METATYPE_INTERFACE_INIT_IMPL(Type, DATASTREAM_DELEGATE) \
{ \
    DATASTREAM_DELEGATE(Type) \
    /*constructor*/(QtMetaTypePrivate::QMetaTypeFunctionHelper<Type, QtMetaTypePrivate::TypeDefinition<Type>::IsAvailable>::Construct), \
    /*destructor*/(QtMetaTypePrivate::QMetaTypeFunctionHelper<Type, QtMetaTypePrivate::TypeDefinition<Type>::IsAvailable>::Destruct), \
    /*size*/(QTypeInfo<Type>::sizeOf), \
    /*flags*/QtPrivate::QMetaTypeTypeFlags<Type>::Flags, \
    /*metaObject*/METAOBJECT_DELEGATE(Type), \
    /*typedConstructor*/ nullptr, \
    /*typedDestructor*/ nullptr \
}


/* These  QT_METATYPE_INTERFACE_INIT* macros are used to initialize QMetaTypeInterface instance.

 - QT_METATYPE_INTERFACE_INIT(Type) -> It takes Type argument and creates all necessary wrapper functions for the Type,
   it detects if QT_NO_DATASTREAM was defined. Probably it is the macro that you want to use.

 - QT_METATYPE_INTERFACE_INIT_EMPTY() -> It initializes an empty QMetaTypeInterface instance.

 - QT_METATYPE_INTERFACE_INIT_NO_DATASTREAM(Type) -> Temporary workaround for missing auto-detection of data stream
   operators. It creates same instance as QT_METATYPE_INTERFACE_INIT(Type) but with null stream operators callbacks.
 */
#define QT_METATYPE_INTERFACE_INIT(Type) QT_METATYPE_INTERFACE_INIT_IMPL(Type, QT_METATYPE_INTERFACE_INIT_DATASTREAM_IMPL)
#define QT_METATYPE_INTERFACE_INIT_NO_DATASTREAM(Type) QT_METATYPE_INTERFACE_INIT_IMPL(Type, QT_METATYPE_INTERFACE_INIT_EMPTY_DATASTREAM_IMPL)
#define QT_METATYPE_INTERFACE_INIT_EMPTY() \
{ \
    QT_METATYPE_INTERFACE_INIT_EMPTY_DATASTREAM_IMPL(void) \
    /*constructor*/ nullptr, \
    /*destructor*/ nullptr, \
    /*size*/ 0, \
    /*flags*/ 0, \
    /*metaObject*/ nullptr , \
    /*typedConstructor*/ nullptr, \
    /*typedDestructor*/ nullptr \
}

namespace QtMetaTypePrivate {
template<typename T>
struct TypeDefinition {
    static const bool IsAvailable = true;
};

// Ignore these types, as incomplete
#ifdef QT_BOOTSTRAPPED
template<> struct TypeDefinition<QBitArray> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QCborArray> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QCborMap> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QCborSimpleType> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QCborValue> { static const bool IsAvailable = false; };
#if QT_CONFIG(easingcurve)
template<> struct TypeDefinition<QEasingCurve> { static const bool IsAvailable = false; };
#endif
template<> struct TypeDefinition<QJsonArray> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QJsonDocument> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QJsonObject> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QJsonValue> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QUrl> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QByteArrayList> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_GEOM_VARIANT
template<> struct TypeDefinition<QRect> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QRectF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QSize> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QSizeF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QLine> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QLineF> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QPoint> { static const bool IsAvailable = false; };
template<> struct TypeDefinition<QPointF> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_REGEXP
template<> struct TypeDefinition<QRegExp> { static const bool IsAvailable = false; };
#endif
#if !QT_CONFIG(regularexpression)
template<> struct TypeDefinition<QRegularExpression> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_SHORTCUT
template<> struct TypeDefinition<QKeySequence> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_CURSOR
template<> struct TypeDefinition<QCursor> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_MATRIX4X4
template<> struct TypeDefinition<QMatrix4x4> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR2D
template<> struct TypeDefinition<QVector2D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR3D
template<> struct TypeDefinition<QVector3D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR4D
template<> struct TypeDefinition<QVector4D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_QUATERNION
template<> struct TypeDefinition<QQuaternion> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_ICON
template<> struct TypeDefinition<QIcon> { static const bool IsAvailable = false; };
#endif
} //namespace QtMetaTypePrivate

QT_END_NAMESPACE

#endif // QMETATYPE_P_H
