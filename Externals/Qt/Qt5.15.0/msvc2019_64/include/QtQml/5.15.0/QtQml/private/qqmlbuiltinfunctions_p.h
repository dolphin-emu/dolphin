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

#ifndef QQMLBUILTINFUNCTIONS_P_H
#define QQMLBUILTINFUNCTIONS_P_H

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

#include <private/qqmlglobal_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qjsengine_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;

namespace QV4 {

namespace Heap {

struct QtObject : Object {
    void init(QQmlEngine *qmlEngine);
    QObject *platform;
    QObject *application;

    enum { Finished = -1 };
    int enumeratorIterator;
    int keyIterator;

    bool isComplete() const
    { return enumeratorIterator == Finished; }
};

struct ConsoleObject : Object {
    void init();
};

#define QQmlBindingFunctionMembers(class, Member) \
    Member(class, Pointer, FunctionObject *, bindingFunction)
DECLARE_HEAP_OBJECT(QQmlBindingFunction, FunctionObject) {
    DECLARE_MARKOBJECTS(QQmlBindingFunction)
    void init(const QV4::FunctionObject *bindingFunction);
};

}

struct QtObject : Object
{
    V4_OBJECT2(QtObject, Object)

    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver, bool *hasProperty);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);

    static ReturnedValue method_isQtObject(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_rgba(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_hsla(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_hsva(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_colorEqual(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_font(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_rect(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_point(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_size(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_vector2d(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_vector3d(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_vector4d(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_quaternion(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_matrix4x4(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_lighter(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_darker(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_tint(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_formatDate(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_formatTime(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_formatDateTime(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_openUrlExternally(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_fontFamilies(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_md5(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_btoa(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_atob(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_quit(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_exit(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_resolvedUrl(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_createQmlObject(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_createComponent(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_uiLanguage(const FunctionObject *b, const Value *, const Value *, int);
    static ReturnedValue method_set_uiLanguage(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
#if QT_CONFIG(qml_locale)
    static ReturnedValue method_locale(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
#endif
    static ReturnedValue method_binding(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_get_platform(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_application(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_inputMethod(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_get_styleHints(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue method_callLater(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

private:
    void addAll();
    ReturnedValue findAndAdd(const QString *name, bool &foundProperty) const;
};

struct ConsoleObject : Object
{
    V4_OBJECT2(ConsoleObject, Object)

    static ReturnedValue method_error(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_log(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_info(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_profile(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_profileEnd(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_time(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_timeEnd(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_count(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_trace(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_warn(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_assert(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_exception(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

};

struct Q_QML_PRIVATE_EXPORT GlobalExtensions {
    static void init(Object *globalObject, QJSEngine::Extensions extensions);

#if QT_CONFIG(translation)
    static ReturnedValue method_qsTranslate(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_qsTranslateNoOp(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_qsTr(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_qsTrNoOp(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_qsTrId(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_qsTrIdNoOp(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);
#endif
    static ReturnedValue method_gc(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

    // on String:prototype
    static ReturnedValue method_string_arg(const FunctionObject *b, const Value *thisObject, const Value *argv, int argc);

};

struct QQmlBindingFunction : public QV4::FunctionObject
{
    V4_OBJECT2(QQmlBindingFunction, FunctionObject)

    Heap::FunctionObject *bindingFunction() const { return d()->bindingFunction; }
    QQmlSourceLocation currentLocation() const; // from caller stack trace
};

inline bool FunctionObject::isBinding() const
{
    return d()->vtable() == QQmlBindingFunction::staticVTable();
}

}

QT_END_NAMESPACE

#endif // QQMLBUILTINFUNCTIONS_P_H
