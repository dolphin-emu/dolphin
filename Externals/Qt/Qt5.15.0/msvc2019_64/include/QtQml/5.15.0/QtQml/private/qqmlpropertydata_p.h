/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QQMLPROPERTYDATA_P_H
#define QQMLPROPERTYDATA_P_H

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

#include <private/qobject_p.h>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QQmlPropertyCacheMethodArguments;
class QQmlPropertyData
{
public:
    enum WriteFlag {
        BypassInterceptor = 0x01,
        DontRemoveBinding = 0x02,
        RemoveBindingOnAliasWrite = 0x04
    };
    Q_DECLARE_FLAGS(WriteFlags, WriteFlag)

    typedef QObjectPrivate::StaticMetaCallFunction StaticMetaCallFunction;

    struct Flags {
        friend class QQmlPropertyData;
        enum Types {
            OtherType            = 0,
            FunctionType         = 1, // Is an invokable
            QObjectDerivedType   = 2, // Property type is a QObject* derived type
            EnumType             = 3, // Property type is an enum
            QListType            = 4, // Property type is a QML list
            QmlBindingType       = 5, // Property type is a QQmlBinding*
            QJSValueType         = 6, // Property type is a QScriptValue
                                      // Gap, used to be V4HandleType
            VarPropertyType      = 8, // Property type is a "var" property of VMEMO
            QVariantType         = 9  // Property is a QVariant
        };

        // The _otherBits (which "pad" the Flags struct to align it nicely) are used
        // to store the relative property index. It will only get used when said index fits. See
        // trySetStaticMetaCallFunction for details.
        // (Note: this padding is done here, because certain compilers have surprising behavior
        // when an enum is declared in-between two bit fields.)
        enum { BitsLeftInFlags = 15 };
        unsigned otherBits       : BitsLeftInFlags; // align to 32 bits

        // Members of the form aORb can only be a when type is not FunctionType, and only be
        // b when type equals FunctionType. For that reason, the semantic meaning of the bit is
        // overloaded, and the accessor functions are used to get the correct value
        //
        // Moreover, isSignalHandler, isOverload and isCloned and isConstructor make only sense
        // for functions, too (and could at a later point be reused for flags that only make sense
        // for non-functions)
        //
        // Lastly, isDirect and isOverridden apply to both functions and non-functions
    private:
        unsigned isConstantORisVMEFunction     : 1; // Has CONST flag OR Function was added by QML
        unsigned isWritableORhasArguments      : 1; // Has WRITE function OR Function takes arguments
        unsigned isResettableORisSignal        : 1; // Has RESET function OR Function is a signal
        unsigned isAliasORisVMESignal          : 1; // Is a QML alias to another property OR Signal was added by QML
        unsigned isFinalORisV4Function         : 1; // Has FINAL flag OR Function takes QQmlV4Function* args
        unsigned isSignalHandler               : 1; // Function is a signal handler
        unsigned isOverload                    : 1; // Function is an overload of another function
        unsigned isRequiredORisCloned          : 1; // Has REQUIRED flag OR The function was marked as cloned
        unsigned isConstructor                 : 1; // The function was marked is a constructor
        unsigned isDirect                      : 1; // Exists on a C++ QMetaObject
        unsigned isOverridden                  : 1; // Is overridden by a extension property
    public:
        unsigned type             : 4; // stores an entry of Types

        // Apply only to IsFunctions

        // Internal QQmlPropertyCache flags
        unsigned notFullyResolved : 1; // True if the type data is to be lazily resolved
        unsigned overrideIndexIsProperty: 1;

        inline Flags();
        inline bool operator==(const Flags &other) const;
        inline void copyPropertyTypeFlags(Flags from);

        void setIsConstant(bool b) {
            Q_ASSERT(type != FunctionType);
            isConstantORisVMEFunction = b;
        }

        void setIsWritable(bool b) {
            Q_ASSERT(type != FunctionType);
            isWritableORhasArguments = b;
        }

        void setIsResettable(bool b) {
            Q_ASSERT(type != FunctionType);
            isResettableORisSignal = b;
        }

        void setIsAlias(bool b) {
            Q_ASSERT(type != FunctionType);
            isAliasORisVMESignal = b;
        }

        void setIsFinal(bool b) {
            Q_ASSERT(type != FunctionType);
            isFinalORisV4Function = b;
        }

        void setIsOverridden(bool b) {
            isOverridden = b;
        }

        void setIsDirect(bool b) {
            isDirect = b;
        }

        void setIsRequired(bool b) {
            Q_ASSERT(type != FunctionType);
            isRequiredORisCloned = b;
        }

        void setIsVMEFunction(bool b) {
            Q_ASSERT(type == FunctionType);
            isConstantORisVMEFunction = b;
        }
        void setHasArguments(bool b) {
            Q_ASSERT(type == FunctionType);
            isWritableORhasArguments = b;
        }
        void setIsSignal(bool b) {
            Q_ASSERT(type == FunctionType);
            isResettableORisSignal = b;
        }
        void setIsVMESignal(bool b) {
            Q_ASSERT(type == FunctionType);
            isAliasORisVMESignal = b;
        }

        void setIsV4Function(bool b) {
            Q_ASSERT(type == FunctionType);
            isFinalORisV4Function = b;
        }

        void setIsSignalHandler(bool b) {
            Q_ASSERT(type == FunctionType);
            isSignalHandler = b;
        }

        void setIsOverload(bool b) {
            Q_ASSERT(type == FunctionType);
            isOverload = b;
        }

        void setIsCloned(bool b) {
            Q_ASSERT(type == FunctionType);
            isRequiredORisCloned = b;
        }

        void setIsConstructor(bool b) {
            Q_ASSERT(type == FunctionType);
            isConstructor = b;
        }

    };


    inline bool operator==(const QQmlPropertyData &) const;

    Flags flags() const { return m_flags; }
    void setFlags(Flags f)
    {
        unsigned otherBits = m_flags.otherBits;
        m_flags = f;
        m_flags.otherBits = otherBits;
    }

    bool isValid() const { return coreIndex() != -1; }

    bool isConstant() const { return !isFunction() && m_flags.isConstantORisVMEFunction; }
    bool isWritable() const { return !isFunction() && m_flags.isWritableORhasArguments; }
    void setWritable(bool onoff) { Q_ASSERT(!isFunction()); m_flags.isWritableORhasArguments = onoff; }
    bool isResettable() const { return !isFunction() && m_flags.isResettableORisSignal; }
    bool isAlias() const { return !isFunction() && m_flags.isAliasORisVMESignal; }
    bool isFinal() const { return !isFunction() && m_flags.isFinalORisV4Function; }
    bool isOverridden() const { return m_flags.isOverridden; }
    bool isDirect() const { return m_flags.isOverload; }
    bool isRequired() const { return !isFunction() && m_flags.isRequiredORisCloned; }
    bool hasStaticMetaCallFunction() const { return staticMetaCallFunction() != nullptr; }
    bool isFunction() const { return m_flags.type == Flags::FunctionType; }
    bool isQObject() const { return m_flags.type == Flags::QObjectDerivedType; }
    bool isEnum() const { return m_flags.type == Flags::EnumType; }
    bool isQList() const { return m_flags.type == Flags::QListType; }
    bool isQmlBinding() const { return m_flags.type == Flags::QmlBindingType; }
    bool isQJSValue() const { return m_flags.type == Flags::QJSValueType; }
    bool isVarProperty() const { return m_flags.type == Flags::VarPropertyType; }
    bool isQVariant() const { return m_flags.type == Flags::QVariantType; }
    bool isVMEFunction() const { return isFunction() && m_flags.isConstantORisVMEFunction; }
    bool hasArguments() const { return isFunction() && m_flags.isWritableORhasArguments; }
    bool isSignal() const { return isFunction() && m_flags.isResettableORisSignal; }
    bool isVMESignal() const { return isFunction() && m_flags.isAliasORisVMESignal; }
    bool isV4Function() const { return isFunction() && m_flags.isFinalORisV4Function; }
    bool isSignalHandler() const { return m_flags.isSignalHandler; }
    bool isOverload() const { return m_flags.isOverload; }
    void setOverload(bool onoff) { m_flags.isOverload = onoff; }
    bool isCloned() const { return isFunction() && m_flags.isRequiredORisCloned; }
    bool isConstructor() const { return m_flags.isConstructor; }

    bool hasOverride() const { return overrideIndex() >= 0; }
    bool hasRevision() const { return revision() != 0; }

    bool isFullyResolved() const { return !m_flags.notFullyResolved; }

    int propType() const { Q_ASSERT(isFullyResolved()); return m_propType; }
    void setPropType(int pt)
    {
        Q_ASSERT(pt >= 0);
        Q_ASSERT(pt <= std::numeric_limits<qint16>::max());
        m_propType = quint16(pt);
    }

    int notifyIndex() const { return m_notifyIndex; }
    void setNotifyIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        m_notifyIndex = qint16(idx);
    }

    bool overrideIndexIsProperty() const { return m_flags.overrideIndexIsProperty; }
    void setOverrideIndexIsProperty(bool onoff) { m_flags.overrideIndexIsProperty = onoff; }

    int overrideIndex() const { return m_overrideIndex; }
    void setOverrideIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        m_overrideIndex = qint16(idx);
    }

    int coreIndex() const { return m_coreIndex; }
    void setCoreIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        m_coreIndex = qint16(idx);
    }

    quint8 revision() const { return m_revision; }
    void setRevision(quint8 rev)
    {
        Q_ASSERT(rev <= std::numeric_limits<quint8>::max());
        m_revision = quint8(rev);
    }

    /* If a property is a C++ type, then we store the minor
     * version of this type.
     * This is required to resolve property or signal revisions
     * if this property is used as a grouped property.
     *
     * Test.qml
     * property TextEdit someTextEdit: TextEdit {}
     *
     * Test {
     *   someTextEdit.preeditText: "test" //revision 7
     *   someTextEdit.onEditingFinished: console.log("test") //revision 6
     * }
     *
     * To determine if these properties with revisions are available we need
     * the minor version of TextEdit as imported in Test.qml.
     *
     */

    quint8 typeMinorVersion() const { return m_typeMinorVersion; }
    void setTypeMinorVersion(quint8 rev)
    {
        Q_ASSERT(rev <= std::numeric_limits<quint8>::max());
        m_typeMinorVersion = quint8(rev);
    }

    QQmlPropertyCacheMethodArguments *arguments() const { return m_arguments; }
    void setArguments(QQmlPropertyCacheMethodArguments *args) { m_arguments = args; }

    int metaObjectOffset() const { return m_metaObjectOffset; }
    void setMetaObjectOffset(int off)
    {
        Q_ASSERT(off >= std::numeric_limits<qint16>::min());
        Q_ASSERT(off <= std::numeric_limits<qint16>::max());
        m_metaObjectOffset = qint16(off);
    }

    StaticMetaCallFunction staticMetaCallFunction() const { return m_staticMetaCallFunction; }
    void trySetStaticMetaCallFunction(StaticMetaCallFunction f, unsigned relativePropertyIndex)
    {
        if (relativePropertyIndex < (1 << Flags::BitsLeftInFlags) - 1) {
            m_flags.otherBits = relativePropertyIndex;
            m_staticMetaCallFunction = f;
        }
    }
    quint16 relativePropertyIndex() const { Q_ASSERT(hasStaticMetaCallFunction()); return m_flags.otherBits; }

    static Flags flagsForProperty(const QMetaProperty &);
    void load(const QMetaProperty &);
    void load(const QMetaMethod &);
    QString name(QObject *) const;
    QString name(const QMetaObject *) const;

    void markAsOverrideOf(QQmlPropertyData *predecessor);

    inline void readProperty(QObject *target, void *property) const
    {
        void *args[] = { property, nullptr };
        readPropertyWithArgs(target, args);
    }

    inline void readPropertyWithArgs(QObject *target, void *args[]) const
    {
        if (hasStaticMetaCallFunction())
            staticMetaCallFunction()(target, QMetaObject::ReadProperty, relativePropertyIndex(), args);
        else if (isDirect())
            target->qt_metacall(QMetaObject::ReadProperty, coreIndex(), args);
        else
            QMetaObject::metacall(target, QMetaObject::ReadProperty, coreIndex(), args);
    }

    bool writeProperty(QObject *target, void *value, WriteFlags flags) const
    {
        int status = -1;
        void *argv[] = { value, nullptr, &status, &flags };
        if (flags.testFlag(BypassInterceptor) && hasStaticMetaCallFunction())
            staticMetaCallFunction()(target, QMetaObject::WriteProperty, relativePropertyIndex(), argv);
        else if (flags.testFlag(BypassInterceptor) && isDirect())
            target->qt_metacall(QMetaObject::WriteProperty, coreIndex(), argv);
        else
            QMetaObject::metacall(target, QMetaObject::WriteProperty, coreIndex(), argv);
        return true;
    }

    static Flags defaultSignalFlags()
    {
        Flags f;
        f.type = Flags::FunctionType;
        f.setIsSignal(true);
        f.setIsVMESignal(true);
        return f;
    }

    static Flags defaultSlotFlags()
    {
        Flags f;
        f.type = Flags::FunctionType;
        f.setIsVMEFunction(true);
        return f;
    }

private:
    friend class QQmlPropertyCache;
    void lazyLoad(const QMetaProperty &);
    void lazyLoad(const QMetaMethod &);
    bool notFullyResolved() const { return m_flags.notFullyResolved; }

    Flags m_flags;
    qint16 m_coreIndex = -1;
    quint16 m_propType = 0;

    // The notify index is in the range returned by QObjectPrivate::signalIndex().
    // This is different from QMetaMethod::methodIndex().
    qint16 m_notifyIndex = -1;
    qint16 m_overrideIndex = -1;

    quint8 m_revision = 0;
    quint8 m_typeMinorVersion = 0;
    qint16 m_metaObjectOffset = -1;

    QQmlPropertyCacheMethodArguments *m_arguments = nullptr;
    StaticMetaCallFunction m_staticMetaCallFunction = nullptr;
};

#if QT_POINTER_SIZE == 4
    Q_STATIC_ASSERT(sizeof(QQmlPropertyData) == 24);
#else // QT_POINTER_SIZE == 8
    Q_STATIC_ASSERT(sizeof(QQmlPropertyData) == 32);
#endif

bool QQmlPropertyData::operator==(const QQmlPropertyData &other) const
{
    return flags() == other.flags() &&
            propType() == other.propType() &&
            coreIndex() == other.coreIndex() &&
            notifyIndex() == other.notifyIndex() &&
            revision() == other.revision();
}

QQmlPropertyData::Flags::Flags()
    : otherBits(0)
    , isConstantORisVMEFunction(false)
    , isWritableORhasArguments(false)
    , isResettableORisSignal(false)
    , isAliasORisVMESignal(false)
    , isFinalORisV4Function(false)
    , isSignalHandler(false)
    , isOverload(false)
    , isRequiredORisCloned(false)
    , isConstructor(false)
    , isOverridden(false)
    , type(OtherType)
    , notFullyResolved(false)
    , overrideIndexIsProperty(false)
{}

bool QQmlPropertyData::Flags::operator==(const QQmlPropertyData::Flags &other) const
{
    return isConstantORisVMEFunction == other.isConstantORisVMEFunction &&
            isWritableORhasArguments == other.isWritableORhasArguments &&
            isResettableORisSignal == other.isResettableORisSignal &&
            isAliasORisVMESignal == other.isAliasORisVMESignal &&
            isFinalORisV4Function == other.isFinalORisV4Function &&
            isOverridden == other.isOverridden &&
            isSignalHandler == other.isSignalHandler &&
            isRequiredORisCloned == other.isRequiredORisCloned &&
            type == other.type &&
            isConstructor == other.isConstructor &&
            notFullyResolved == other.notFullyResolved &&
            overrideIndexIsProperty == other.overrideIndexIsProperty;
}

void QQmlPropertyData::Flags::copyPropertyTypeFlags(QQmlPropertyData::Flags from)
{
    switch (from.type) {
    case QObjectDerivedType:
    case EnumType:
    case QListType:
    case QmlBindingType:
    case QJSValueType:
    case QVariantType:
        type = from.type;
    }
}

Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlPropertyData::WriteFlags)

QT_END_NAMESPACE

#endif // QQMLPROPERTYDATA_P_H
