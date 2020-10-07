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

#ifndef QQMLBINDING_P_H
#define QQMLBINDING_P_H

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

#include "qqml.h"
#include "qqmlpropertyvaluesource.h"
#include "qqmlexpression.h"
#include "qqmlproperty.h"
#include "qqmlscriptstring.h"
#include "qqmlproperty_p.h"

#include <QtCore/QObject>
#include <QtCore/QMetaProperty>

#include <private/qqmlabstractbinding_p.h>
#include <private/qqmljavascriptexpression_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlContext;
class Q_QML_PRIVATE_EXPORT QQmlBinding : public QQmlJavaScriptExpression,
                                         public QQmlAbstractBinding
{
    friend class QQmlAbstractBinding;
public:
    typedef QExplicitlySharedDataPointer<QQmlBinding> Ptr;

    static QQmlBinding *create(const QQmlPropertyData *, const QQmlScriptString &, QObject *, QQmlContext *);
    static QQmlBinding *create(const QQmlPropertyData *, const QString &, QObject *, QQmlContextData *,
                               const QString &url = QString(), quint16 lineNumber = 0);
    static QQmlBinding *create(const QQmlPropertyData *property, QV4::Function *function,
                               QObject *obj, QQmlContextData *ctxt, QV4::ExecutionContext *scope);
    static QQmlBinding *createTranslationBinding(const QQmlRefPointer<QV4::ExecutableCompilationUnit> &unit, const QV4::CompiledData::Binding *binding,
                                                 QObject *obj, QQmlContextData *ctxt);
    ~QQmlBinding() override;

    void setTarget(const QQmlProperty &);
    bool setTarget(QObject *, const QQmlPropertyData &, const QQmlPropertyData *valueType);

    void setNotifyOnValueChanged(bool);

    void refresh() override;

    void setEnabled(bool, QQmlPropertyData::WriteFlags flags = QQmlPropertyData::DontRemoveBinding) override;
    QString expression() const override;
    void update(QQmlPropertyData::WriteFlags flags = QQmlPropertyData::DontRemoveBinding);

    typedef int Identifier;
    enum {
        Invalid = -1
    };

    QVariant evaluate();

    QString expressionIdentifier() const override;
    void expressionChanged() override;

    QQmlSourceLocation sourceLocation() const override;
    void setSourceLocation(const QQmlSourceLocation &location);
    void setBoundFunction(QV4::BoundFunction *boundFunction) {
        m_boundFunction.set(boundFunction->engine(), *boundFunction);
    }

    /**
     * This method returns a snapshot of the currently tracked dependencies of
     * this binding. The dependencies can change upon reevaluation. This method is
     * used in GammaRay to visualize binding hierarchies.
     *
     * Call this method from the UI thread.
     */
    QVector<QQmlProperty> dependencies() const;
    virtual bool hasDependencies() const;

protected:
    virtual void doUpdate(const DeleteWatcher &watcher,
                          QQmlPropertyData::WriteFlags flags, QV4::Scope &scope) = 0;

    void getPropertyData(QQmlPropertyData **propertyData, QQmlPropertyData *valueTypeData) const;
    int getPropertyType() const;

    bool slowWrite(const QQmlPropertyData &core, const QQmlPropertyData &valueTypeData,
                   const QV4::Value &result, bool isUndefined, QQmlPropertyData::WriteFlags flags);

    QV4::ReturnedValue evaluate(bool *isUndefined);

private:
    inline bool updatingFlag() const;
    inline void setUpdatingFlag(bool);
    inline bool enabledFlag() const;
    inline void setEnabledFlag(bool);

    static QQmlBinding *newBinding(QQmlEnginePrivate *engine, const QQmlPropertyData *property);

    QQmlSourceLocation *m_sourceLocation = nullptr; // used for Qt.binding() created functions
    QV4::PersistentValue m_boundFunction; // used for Qt.binding() that are created from a bound function object
};

bool QQmlBinding::updatingFlag() const
{
    return m_target.flag();
}

void QQmlBinding::setUpdatingFlag(bool v)
{
    m_target.setFlagValue(v);
}

bool QQmlBinding::enabledFlag() const
{
    return m_target.flag2();
}

void QQmlBinding::setEnabledFlag(bool v)
{
    m_target.setFlag2Value(v);
}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlBinding*)

#endif // QQMLBINDING_P_H
