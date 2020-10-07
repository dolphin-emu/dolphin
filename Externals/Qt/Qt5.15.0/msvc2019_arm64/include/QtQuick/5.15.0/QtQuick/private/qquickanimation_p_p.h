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

#ifndef QQUICKANIMATION2_P_H
#define QQUICKANIMATION2_P_H

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

#include "qquickanimation_p.h"

#include <private/qqmlnullablevalue_p.h>

#include <qqml.h>
#include <qqmlcontext.h>

#include <private/qvariantanimation_p.h>
#include "private/qpauseanimationjob_p.h"
#include <QDebug>

#include <private/qobject_p.h>
#include "private/qanimationgroupjob_p.h"
#include <QDebug>

#include <private/qobject_p.h>


QT_BEGIN_NAMESPACE

//interface for classes that provide animation actions for QActionAnimation
class QAbstractAnimationAction
{
public:
    virtual ~QAbstractAnimationAction() {}
    virtual void doAction() = 0;
    virtual void debugAction(QDebug, int) const {}
};

//templated animation action
//allows us to specify an action that calls a function of a class.
//(so that class doesn't have to inherit QQuickAbstractAnimationAction)
template<class T, void (T::*method)(), void (T::*debugMethod)(QDebug, int) const>
class QAnimationActionProxy : public QAbstractAnimationAction
{
public:
    QAnimationActionProxy(T *instance) : m_instance(instance) {}
    void doAction() override { (m_instance->*method)(); }
    void debugAction(QDebug d, int indentLevel) const override { (m_instance->*debugMethod)(d, indentLevel); }
private:
    T *m_instance;
};

//performs an action of type QAbstractAnimationAction
class Q_AUTOTEST_EXPORT QActionAnimation : public QAbstractAnimationJob
{
    Q_DISABLE_COPY(QActionAnimation)
public:
    QActionAnimation();

    QActionAnimation(QAbstractAnimationAction *action);
    ~QActionAnimation() override;

    int duration() const override;
    void setAnimAction(QAbstractAnimationAction *action);

protected:
    void updateCurrentTime(int) override;
    void updateState(State newState, State oldState) override;
    void debugAnimation(QDebug d) const override;

private:
    QAbstractAnimationAction *animAction;
};

class QQuickBulkValueUpdater
{
public:
    virtual ~QQuickBulkValueUpdater() {}
    virtual void setValue(qreal value) = 0;
    virtual void debugUpdater(QDebug, int) const {}
};

//animates QQuickBulkValueUpdater (assumes start and end values will be reals or compatible)
class Q_AUTOTEST_EXPORT QQuickBulkValueAnimator : public QAbstractAnimationJob
{
    Q_DISABLE_COPY(QQuickBulkValueAnimator)
public:
    QQuickBulkValueAnimator();
    ~QQuickBulkValueAnimator() override;

    void setAnimValue(QQuickBulkValueUpdater *value);
    QQuickBulkValueUpdater *getAnimValue() const { return animValue; }

    void setFromSourcedValue(bool *value) { fromSourced = value; }

    int duration() const override { return m_duration; }
    void setDuration(int msecs) { m_duration = msecs; }

    QEasingCurve easingCurve() const { return easing; }
    void setEasingCurve(const QEasingCurve &curve) { easing = curve; }

protected:
    void updateCurrentTime(int currentTime) override;
    void topLevelAnimationLoopChanged() override;
    void debugAnimation(QDebug d) const override;

private:
    QQuickBulkValueUpdater *animValue;
    bool *fromSourced;
    int m_duration;
    QEasingCurve easing;
};

//an animation that just gives a tick
template<class T, void (T::*method)(int)>
class QTickAnimationProxy : public QAbstractAnimationJob
{
    Q_DISABLE_COPY(QTickAnimationProxy)
public:
    QTickAnimationProxy(T *instance) : QAbstractAnimationJob(), m_instance(instance) {}
    int duration() const override { return -1; }
protected:
    void updateCurrentTime(int msec) override { (m_instance->*method)(msec); }

private:
    T *m_instance;
};

class Q_QUICK_PRIVATE_EXPORT QQuickAbstractAnimationPrivate : public QObjectPrivate, public QAnimationJobChangeListener
{
    Q_DECLARE_PUBLIC(QQuickAbstractAnimation)
public:
    QQuickAbstractAnimationPrivate()
    : running(false), paused(false), alwaysRunToEnd(false),
      /*connectedTimeLine(false), */componentComplete(true),
      avoidPropertyValueSourceStart(false), disableUserControl(false),
      registered(false), loopCount(1), group(nullptr), animationInstance(nullptr) {}

    bool running:1;
    bool paused:1;
    bool alwaysRunToEnd:1;
    //bool connectedTimeLine:1;
    bool componentComplete:1;
    bool avoidPropertyValueSourceStart:1;
    bool disableUserControl:1;
    bool registered:1;

    int loopCount;

    void commence();
    void animationFinished(QAbstractAnimationJob *) override;

    QQmlProperty defaultProperty;

    QQuickAnimationGroup *group;
    QAbstractAnimationJob* animationInstance;

    static QQmlProperty createProperty(QObject *obj, const QString &str, QObject *infoObj, QString *errorMessage = nullptr);
};

class QQuickPauseAnimationPrivate : public QQuickAbstractAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickPauseAnimation)
public:
    QQuickPauseAnimationPrivate()
        : QQuickAbstractAnimationPrivate(), duration(250) {}

    int duration;
};

class QQuickScriptActionPrivate : public QQuickAbstractAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickScriptAction)
public:
    QQuickScriptActionPrivate();

    QQmlScriptString script;
    QString name;
    QQmlScriptString runScriptScript;
    bool hasRunScriptScript;
    bool reversing;

    void execute();
    QAbstractAnimationAction* createAction();
    void debugAction(QDebug d, int indentLevel) const;
    typedef QAnimationActionProxy<QQuickScriptActionPrivate,
                                 &QQuickScriptActionPrivate::execute,
                                 &QQuickScriptActionPrivate::debugAction> Proxy;
};

class QQuickPropertyActionPrivate : public QQuickAbstractAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickPropertyAction)
public:
    QQuickPropertyActionPrivate()
    : QQuickAbstractAnimationPrivate(), target(nullptr) {}

    QObject *target;
    QString propertyName;
    QString properties;
    QList<QObject *> targets;
    QList<QObject *> exclude;

    QQmlNullableValue<QVariant> value;
};

class QQuickAnimationGroupPrivate : public QQuickAbstractAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickAnimationGroup)
public:
    QQuickAnimationGroupPrivate()
    : QQuickAbstractAnimationPrivate() {}

    static void append_animation(QQmlListProperty<QQuickAbstractAnimation> *list, QQuickAbstractAnimation *role);
    static QQuickAbstractAnimation *at_animation(QQmlListProperty<QQuickAbstractAnimation> *list, int index);
    static int count_animation(QQmlListProperty<QQuickAbstractAnimation> *list);
    static void clear_animation(QQmlListProperty<QQuickAbstractAnimation> *list);
    static void replace_animation(QQmlListProperty<QQuickAbstractAnimation> *list, int index,
                                  QQuickAbstractAnimation *role);
    static void removeLast_animation(QQmlListProperty<QQuickAbstractAnimation> *list);
    QList<QQuickAbstractAnimation *> animations;
};

class QQuickPropertyAnimationPrivate : public QQuickAbstractAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickPropertyAnimation)
public:
    QQuickPropertyAnimationPrivate()
    : QQuickAbstractAnimationPrivate(), target(nullptr), fromSourced(false), fromIsDefined(false), toIsDefined(false),
      defaultToInterpolatorType(0), interpolatorType(0), interpolator(nullptr), duration(250), actions(nullptr) {}

    QVariant from;
    QVariant to;

    QObject *target;
    QString propertyName;
    QString properties;
    QList<QObject *> targets;
    QList<QObject *> exclude;
    QString defaultProperties;

    bool fromSourced;
    bool fromIsDefined:1;
    bool toIsDefined:1;
    bool defaultToInterpolatorType:1;
    int interpolatorType;
    QVariantAnimation::Interpolator interpolator;
    int duration;
    QEasingCurve easing;

    // for animations that don't use the QQuickBulkValueAnimator
    QQuickStateActions *actions;

    static QVariant interpolateVariant(const QVariant &from, const QVariant &to, qreal progress);
    static void convertVariant(QVariant &variant, int type);
};

class QQuickRotationAnimationPrivate : public QQuickPropertyAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickRotationAnimation)
public:
    QQuickRotationAnimationPrivate() : direction(QQuickRotationAnimation::Numerical) {}

    QQuickRotationAnimation::RotationDirection direction;
};

class Q_AUTOTEST_EXPORT QQuickAnimationPropertyUpdater : public QQuickBulkValueUpdater
{
public:
    QQuickAnimationPropertyUpdater() : interpolatorType(0), interpolator(nullptr), prevInterpolatorType(0), reverse(false), fromSourced(false), fromDefined(false), wasDeleted(nullptr) {}
    ~QQuickAnimationPropertyUpdater() override;

    void setValue(qreal v) override;

    void debugUpdater(QDebug d, int indentLevel) const override;

    QQuickStateActions actions;
    int interpolatorType;       //for Number/ColorAnimation
    QVariantAnimation::Interpolator interpolator;
    int prevInterpolatorType;   //for generic
    bool reverse;
    bool fromSourced;
    bool fromDefined;
    bool *wasDeleted;
};

QT_END_NAMESPACE

#endif // QQUICKANIMATION2_P_H
