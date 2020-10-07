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
#ifndef QV4PROPERTYDESCRIPTOR_H
#define QV4PROPERTYDESCRIPTOR_H

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

#include "qv4global_p.h"
#include "qv4value_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct FunctionObject;

struct Property {
    Value value;
    Value set;

    // Section 8.10
    inline void fullyPopulated(PropertyAttributes *attrs) {
        if (!attrs->hasType()) {
            value = Value::undefinedValue();
        }
        if (attrs->type() == PropertyAttributes::Accessor) {
            attrs->clearWritable();
            if (value.isEmpty())
                value = Value::undefinedValue();
            if (set.isEmpty())
                set = Value::undefinedValue();
        }
        attrs->resolve();
    }

    // ES8: 6.2.5.6
    void completed(PropertyAttributes *attrs) {
        if (value.isEmpty())
            value = Encode::undefined();
        if (attrs->isGeneric() || attrs->isData()) {
            attrs->setType(PropertyAttributes::Data);
            if (!attrs->hasWritable())
                attrs->setWritable(false);
        } else {
            if (set.isEmpty())
                set = Encode::undefined();
        }
        if (!attrs->hasEnumerable())
            attrs->setEnumerable(false);
        if (!attrs->hasConfigurable())
            attrs->setConfigurable(false);
    }

    inline bool isSubset(const PropertyAttributes &attrs, const Property *other, PropertyAttributes otherAttrs) const;
    inline void merge(PropertyAttributes &attrs, const Property *other, PropertyAttributes otherAttrs);

    inline Heap::FunctionObject *getter() const { return reinterpret_cast<Heap::FunctionObject *>(value.heapObject()); }
    inline Heap::FunctionObject *setter() const { return reinterpret_cast<Heap::FunctionObject *>(set.heapObject()); }
    inline void setGetter(FunctionObject *g) { value = reinterpret_cast<Managed *>(g); }
    inline void setSetter(FunctionObject *s) { set = (s ? reinterpret_cast<Managed *>(s) : nullptr); }

    void copy(const Property *other, PropertyAttributes attrs) {
        value = other->value;
        if (attrs.isAccessor())
            set = other->set;
    }

    // ES8, section 9.1.6.2/9,.1.6.3
    bool isCompatible(PropertyAttributes &attrs, const Property *other, PropertyAttributes otherAttrs) const {
        if (otherAttrs.isEmpty())
            return true;
        if (!attrs.isConfigurable()) {
            if (otherAttrs.hasConfigurable() && otherAttrs.isConfigurable())
                return false;
            if (otherAttrs.hasEnumerable() && otherAttrs.isEnumerable() != attrs.isEnumerable())
                return false;
        }
        if (otherAttrs.isGeneric())
            return true;
        if (attrs.isData() != otherAttrs.isData()) {
            if (!attrs.isConfigurable())
                return false;
        } else if (attrs.isData() && otherAttrs.isData()) {
            if (!attrs.isConfigurable() && !attrs.isWritable()) {
                if (otherAttrs.hasWritable() && otherAttrs.isWritable())
                    return false;
                if (!other->value.isEmpty() && !value.sameValue(other->value))
                    return false;
            }
        } else if (attrs.isAccessor() && otherAttrs.isAccessor()) {
            if (!attrs.isConfigurable()) {
                if (!other->value.isEmpty() && !value.sameValue(other->value))
                    return false;
                if (!other->set.isEmpty() && !set.sameValue(other->set))
                    return false;
            }
        }
        return true;
    }


    explicit Property()  { value = Encode::undefined(); set = Value::fromHeapObject(nullptr); }
    Property(Heap::FunctionObject *getter, Heap::FunctionObject *setter) {
        value.setM(reinterpret_cast<Heap::Base *>(getter));
        set.setM(reinterpret_cast<Heap::Base *>(setter));
    }
private:
    Q_DISABLE_COPY(Property)
};

inline bool Property::isSubset(const PropertyAttributes &attrs, const Property *other, PropertyAttributes otherAttrs) const
{
    if (attrs.type() != PropertyAttributes::Generic && attrs.type() != otherAttrs.type())
        return false;
    if (attrs.hasEnumerable() && attrs.isEnumerable() != otherAttrs.isEnumerable())
        return false;
    if (attrs.hasConfigurable() && attrs.isConfigurable() != otherAttrs.isConfigurable())
        return false;
    if (attrs.hasWritable() && attrs.isWritable() != otherAttrs.isWritable())
        return false;
    if (attrs.type() == PropertyAttributes::Data && !value.sameValue(other->value))
        return false;
    if (attrs.type() == PropertyAttributes::Accessor) {
        if (value.heapObject() != other->value.heapObject())
            return false;
        if (set.heapObject() != other->set.heapObject())
            return false;
    }
    return true;
}

inline void Property::merge(PropertyAttributes &attrs, const Property *other, PropertyAttributes otherAttrs)
{
    if (otherAttrs.hasEnumerable())
        attrs.setEnumerable(otherAttrs.isEnumerable());
    if (otherAttrs.hasConfigurable())
        attrs.setConfigurable(otherAttrs.isConfigurable());
    if (otherAttrs.hasWritable())
        attrs.setWritable(otherAttrs.isWritable());
    if (otherAttrs.type() == PropertyAttributes::Accessor) {
        attrs.setType(PropertyAttributes::Accessor);
        if (!other->value.isEmpty())
            value = other->value;
        if (!other->set.isEmpty())
            set = other->set;
    } else if (otherAttrs.type() == PropertyAttributes::Data){
        attrs.setType(PropertyAttributes::Data);
        value = other->value;
    }
}

struct PropertyIndex {
    Heap::Base *base;
    Value *slot;

    void set(EngineBase *e, Value newVal) {
        WriteBarrier::write(e, base, slot->data_ptr(), newVal.asReturnedValue());
    }
    const Value *operator->() const { return slot; }
    const Value &operator*() const { return *slot; }
    bool isNull() const { return !slot; }
};


}

Q_DECLARE_TYPEINFO(QV4::Property, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
