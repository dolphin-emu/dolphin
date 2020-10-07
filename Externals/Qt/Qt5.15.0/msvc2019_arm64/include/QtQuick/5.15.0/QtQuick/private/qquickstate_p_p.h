/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKSTATE_P_H
#define QQUICKSTATE_P_H

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

#include "qquickstate_p.h"

#include "qquicktransitionmanager_p_p.h"

#include <private/qqmlproperty_p.h>
#include <private/qqmlguard_p.h>

#include <private/qqmlbinding_p.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQuickSimpleAction
{
public:
    enum State { StartState, EndState };
    QQuickSimpleAction(const QQuickStateAction &a, State state = StartState)
    {
        m_property = a.property;
        m_specifiedObject = a.specifiedObject;
        m_specifiedProperty = a.specifiedProperty;
        m_event = a.event;
        if (state == StartState) {
            m_value = a.fromValue;
            if (QQmlPropertyPrivate::binding(m_property)) {
                m_binding = QQmlPropertyPrivate::binding(m_property);
            }
            m_reverseEvent = true;
        } else {
            m_value = a.toValue;
            m_binding = a.toBinding;
            m_reverseEvent = false;
        }
    }

    ~QQuickSimpleAction()
    {
    }

    QQuickSimpleAction(const QQuickSimpleAction &other)
        :  m_property(other.m_property),
        m_value(other.m_value),
        m_binding(other.binding()),
        m_specifiedObject(other.m_specifiedObject),
        m_specifiedProperty(other.m_specifiedProperty),
        m_event(other.m_event),
        m_reverseEvent(other.m_reverseEvent)
    {
    }

    QQuickSimpleAction &operator =(const QQuickSimpleAction &other)
    {
        m_property = other.m_property;
        m_value = other.m_value;
        m_binding = other.binding();
        m_specifiedObject = other.m_specifiedObject;
        m_specifiedProperty = other.m_specifiedProperty;
        m_event = other.m_event;
        m_reverseEvent = other.m_reverseEvent;

        return *this;
    }

    void setProperty(const QQmlProperty &property)
    {
        m_property = property;
    }

    const QQmlProperty &property() const
    {
        return m_property;
    }

    void setValue(const QVariant &value)
    {
        m_value = value;
    }

    const QVariant &value() const
    {
        return m_value;
    }

    void setBinding(QQmlAbstractBinding *binding)
    {
        m_binding = binding;
    }

    QQmlAbstractBinding *binding() const
    {
        return m_binding.data();
    }

    QObject *specifiedObject() const
    {
        return m_specifiedObject;
    }

    const QString &specifiedProperty() const
    {
        return m_specifiedProperty;
    }

    QQuickStateActionEvent *event() const
    {
        return m_event;
    }

    bool reverseEvent() const
    {
        return m_reverseEvent;
    }

private:
    QQmlProperty m_property;
    QVariant m_value;
    QQmlAbstractBinding::Ptr m_binding;
    QObject *m_specifiedObject;
    QString m_specifiedProperty;
    QQuickStateActionEvent *m_event;
    bool m_reverseEvent;
};

class QQuickRevertAction
{
public:
    QQuickRevertAction() : event(nullptr) {}
    QQuickRevertAction(const QQmlProperty &prop) : property(prop), event(nullptr) {}
    QQuickRevertAction(QQuickStateActionEvent *e) : event(e) {}
    QQmlProperty property;
    QQuickStateActionEvent *event;
};

class QQuickStateOperationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickStateOperation)

public:

    QQuickStateOperationPrivate()
    : m_state(nullptr) {}

    QQuickState *m_state;
};

class QQuickStatePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickState)

public:
    QQuickStatePrivate()
        : when(false), whenKnown(false), named(false), inState(false), group(nullptr) {}

    typedef QList<QQuickSimpleAction> SimpleActionList;

    QString name;
    bool when;
    bool whenKnown;
    bool named;

    struct OperationGuard : public QQmlGuard<QQuickStateOperation>
    {
        OperationGuard(QObject *obj, QList<OperationGuard> *l) : list(l) {
            setObject(static_cast<QQuickStateOperation *>(obj));
        }
        QList<OperationGuard> *list;
        void objectDestroyed(QQuickStateOperation *) override {
            // we assume priv will always be destroyed after objectDestroyed calls
            list->removeOne(*this);
        }
    };
    QList<OperationGuard> operations;

    static void operations_append(QQmlListProperty<QQuickStateOperation> *prop, QQuickStateOperation *op) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        op->setState(qobject_cast<QQuickState*>(prop->object));
        list->append(OperationGuard(op, list));
    }
    static void operations_clear(QQmlListProperty<QQuickStateOperation> *prop) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        for (auto &e : *list)
            e->setState(nullptr);
        list->clear();
    }
    static int operations_count(QQmlListProperty<QQuickStateOperation> *prop) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        return list->count();
    }
    static QQuickStateOperation *operations_at(QQmlListProperty<QQuickStateOperation> *prop, int index) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        return list->at(index);
    }
    static void operations_replace(QQmlListProperty<QQuickStateOperation> *prop, int index,
                                   QQuickStateOperation *op) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        auto &guard = list->at(index);
        if (guard.object() == op) {
            op->setState(qobject_cast<QQuickState*>(prop->object));
        } else {
            list->at(index)->setState(nullptr);
            op->setState(qobject_cast<QQuickState*>(prop->object));
            list->replace(index, OperationGuard(op, list));
        }
    }
    static void operations_removeLast(QQmlListProperty<QQuickStateOperation> *prop) {
        QList<OperationGuard> *list = static_cast<QList<OperationGuard> *>(prop->data);
        list->last()->setState(nullptr);
        list->removeLast();
    }

    QQuickTransitionManager transitionManager;

    SimpleActionList revertList;
    QList<QQuickRevertAction> reverting;
    QString extends;
    mutable bool inState;
    QQuickStateGroup *group;

    QQuickStateOperation::ActionList generateActionList() const;
    void complete();
};

QT_END_NAMESPACE

#endif // QQUICKSTATE_P_H
