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

#ifndef QQUICKSTATE_H
#define QQUICKSTATE_H

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

#include <qqml.h>
#include <qqmlproperty.h>
#include <QtCore/qobject.h>
#include <QtCore/qsharedpointer.h>
#include <private/qtquickglobal_p.h>
#include <private/qqmlabstractbinding_p.h>

QT_BEGIN_NAMESPACE

class QQuickStateActionEvent;
class QQmlBinding;
class QQmlExpression;

class Q_QUICK_PRIVATE_EXPORT QQuickStateAction
{
public:
    QQuickStateAction();
    QQuickStateAction(QObject *, const QString &, const QVariant &);
    QQuickStateAction(QObject *, const QQmlProperty &property, const QString &,
                      const QVariant &);

    bool restore:1;
    bool actionDone:1;
    bool reverseEvent:1;
    bool deletableToBinding:1;

    QQmlProperty property;
    QVariant fromValue;
    QVariant toValue;

    QQmlAbstractBinding::Ptr fromBinding;
    QQmlAbstractBinding::Ptr toBinding;
    QQuickStateActionEvent *event;

    //strictly for matching
    QObject *specifiedObject;
    QString specifiedProperty;

    void deleteFromBinding();
};

class Q_AUTOTEST_EXPORT QQuickStateActionEvent
{
public:
    virtual ~QQuickStateActionEvent();

    enum EventType { Script, SignalHandler, ParentChange, AnchorChanges };

    virtual EventType type() const = 0;

    virtual void execute();
    virtual bool isReversable();
    virtual void reverse();
    virtual void saveOriginals() {}
    virtual bool needsCopy() { return false; }
    virtual void copyOriginals(QQuickStateActionEvent *) {}

    virtual bool isRewindable() { return isReversable(); }
    virtual void rewind() {}
    virtual void saveCurrentValues() {}
    virtual void saveTargetValues() {}

    virtual bool changesBindings();
    virtual void clearBindings();
    virtual bool mayOverride(QQuickStateActionEvent*other);
};

//### rename to QQuickStateChange?
class QQuickStateGroup;
class QQuickState;
class QQuickStateOperationPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickStateOperation : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    QQuickStateOperation(QObject *parent = nullptr)
        : QObject(parent) {}
    typedef QList<QQuickStateAction> ActionList;

    virtual ActionList actions();

    QQuickState *state() const;
    void setState(QQuickState *state);

protected:
    QQuickStateOperation(QObjectPrivate &dd, QObject *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(QQuickStateOperation)
    Q_DISABLE_COPY(QQuickStateOperation)
};

typedef QQuickStateOperation::ActionList QQuickStateActions;

class QQuickTransition;
class QQuickStatePrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickState : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(bool when READ when WRITE setWhen)
    Q_PROPERTY(QString extend READ extends WRITE setExtends)
    Q_PROPERTY(QQmlListProperty<QQuickStateOperation> changes READ changes)
    Q_CLASSINFO("DefaultProperty", "changes")
    Q_CLASSINFO("DeferredPropertyNames", "changes")
    QML_NAMED_ELEMENT(State)

public:
    QQuickState(QObject *parent=nullptr);
    ~QQuickState() override;

    QString name() const;
    void setName(const QString &);
    bool isNamed() const;

    bool isWhenKnown() const;
    bool when() const;
    void setWhen(bool);

    QString extends() const;
    void setExtends(const QString &);

    QQmlListProperty<QQuickStateOperation> changes();
    int operationCount() const;
    QQuickStateOperation *operationAt(int) const;

    QQuickState &operator<<(QQuickStateOperation *);

    void apply(QQuickTransition *, QQuickState *revert);
    void cancel();

    QQuickStateGroup *stateGroup() const;
    void setStateGroup(QQuickStateGroup *);

    bool containsPropertyInRevertList(QObject *target, const QString &name) const;
    bool changeValueInRevertList(QObject *target, const QString &name, const QVariant &revertValue);
    bool changeBindingInRevertList(QObject *target, const QString &name, QQmlAbstractBinding *binding);
    bool removeEntryFromRevertList(QObject *target, const QString &name);
    void addEntryToRevertList(const QQuickStateAction &action);
    void removeAllEntriesFromRevertList(QObject *target);
    void addEntriesToRevertList(const QList<QQuickStateAction> &actions);
    QVariant valueInRevertList(QObject *target, const QString &name) const;
    QQmlAbstractBinding *bindingInRevertList(QObject *target, const QString &name) const;

    bool isStateActive() const;

Q_SIGNALS:
    void completed();

private:
    Q_DECLARE_PRIVATE(QQuickState)
    Q_DISABLE_COPY(QQuickState)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickStateOperation)
QML_DECLARE_TYPE(QQuickState)

#endif // QQUICKSTATE_H
