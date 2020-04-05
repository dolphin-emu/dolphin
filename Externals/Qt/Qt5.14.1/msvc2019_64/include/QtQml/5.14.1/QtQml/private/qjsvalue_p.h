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

#ifndef QJSVALUE_P_H
#define QJSVALUE_P_H

#include <qjsvalue.h>
#include <private/qtqmlglobal_p.h>
#include <private/qv4value_p.h>
#include <private/qv4string_p.h>
#include <private/qv4engine_p.h>
#include <private/qflagpointer_p.h>
#include <private/qv4mm_p.h>
#include <private/qv4persistent_p.h>

#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT QJSValuePrivate
{
public:
    static inline QV4::Value *getValue(const QJSValue *jsval)
    {
        if (jsval->d & 3)
            return nullptr;
        return reinterpret_cast<QV4::Value *>(jsval->d);
    }

    static inline QVariant *getVariant(const QJSValue *jsval)
    {
        if (jsval->d & 1)
            return reinterpret_cast<QVariant *>(jsval->d & ~3);
        return nullptr;
    }

    static inline void setRawValue(QJSValue *jsval, QV4::Value *v)
    {
        jsval->d = reinterpret_cast<quintptr>(v);
    }

    static inline void setVariant(QJSValue *jsval, const QVariant &v) {
        QVariant *val = new QVariant(v);
        jsval->d = reinterpret_cast<quintptr>(val) | 1;
    }

    static inline void setValue(QJSValue *jsval, QV4::ExecutionEngine *engine, const QV4::Value &v) {
        QV4::Value *value = engine->memoryManager->m_persistentValues->allocate();
        *value = v;
        jsval->d = reinterpret_cast<quintptr>(value);
    }

    static inline void setValue(QJSValue *jsval, QV4::ExecutionEngine *engine, QV4::ReturnedValue v) {
        QV4::Value *value = engine->memoryManager->m_persistentValues->allocate();
        *value = v;
        jsval->d = reinterpret_cast<quintptr>(value);
    }

    static QV4::ReturnedValue convertedToValue(QV4::ExecutionEngine *e, const QJSValue &jsval)
    {
        QV4::Value *v = getValue(&jsval);
        if (!v) {
            QVariant *variant = getVariant(&jsval);
            v = e->memoryManager->m_persistentValues->allocate();
            *v = variant ? e->fromVariant(*variant) : QV4::Encode::undefined();
            jsval.d = reinterpret_cast<quintptr>(v);
            delete variant;
        }

        if (QV4::PersistentValueStorage::getEngine(v) != e) {
            qWarning("JSValue can't be reassigned to another engine.");
            return QV4::Encode::undefined();
        }

        return v->asReturnedValue();
    }

    static QV4::Value *valueForData(const QJSValue *jsval, QV4::Value *scratch)
    {
        QV4::Value *v = getValue(jsval);
        if (v)
            return v;
        v = scratch;
        QVariant *variant = getVariant(jsval);
        if (!variant) {
            *v = QV4::Encode::undefined();
            return v;
        }

        switch (variant->userType()) {
        case QMetaType::UnknownType:
        case QMetaType::Void:
            *v = QV4::Encode::undefined();
            break;
        case QMetaType::Nullptr:
        case QMetaType::VoidStar:
            *v = QV4::Encode::null();
            break;
        case QMetaType::Bool:
            *v = QV4::Encode(variant->toBool());
            break;
        case QMetaType::Double:
            *v = QV4::Encode(variant->toDouble());
            break;
        case QMetaType::Int:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Char:
        case QMetaType::UChar:
            *v = QV4::Encode(variant->toInt());
            break;
        case QMetaType::UInt:
            *v = QV4::Encode(variant->toUInt());
            break;
        default:
            return nullptr;
        }
        return v;
    }

    static QV4::ExecutionEngine *engine(const QJSValue *jsval) {
        QV4::Value *v = getValue(jsval);
        return v ? QV4::PersistentValueStorage::getEngine(v) : nullptr;
    }

    static inline bool checkEngine(QV4::ExecutionEngine *e, const QJSValue &jsval) {
        QV4::ExecutionEngine *v4 = engine(&jsval);
        return !v4 || v4 == e;
    }

    static inline void free(QJSValue *jsval) {
        if (QV4::Value *v = QJSValuePrivate::getValue(jsval)) {
            if (QV4::ExecutionEngine *e = engine(jsval)) {
                if (QJSEngine *jsEngine = e->jsEngine()) {
                    if (jsEngine->thread() != QThread::currentThread()) {
                        QMetaObject::invokeMethod(
                                jsEngine, [v](){ QV4::PersistentValueStorage::free(v); });
                        return;
                    }
                }
            }
            QV4::PersistentValueStorage::free(v);
        } else if (QVariant *v = QJSValuePrivate::getVariant(jsval)) {
            delete v;
        }
    }
};

QT_END_NAMESPACE

#endif
