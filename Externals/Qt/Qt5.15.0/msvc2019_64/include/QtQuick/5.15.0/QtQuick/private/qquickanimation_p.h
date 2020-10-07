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

#ifndef QQUICKANIMATION_H
#define QQUICKANIMATION_H

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
#include <QtGui/qvector3d.h>

#include <qqmlpropertyvaluesource.h>
#include <qqml.h>
#include <qqmlscriptstring.h>

#include <QtCore/qvariant.h>
#include <QtCore/qeasingcurve.h>
#include "private/qabstractanimationjob_p.h"
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QQuickAbstractAnimationPrivate;
class QQuickAnimationGroup;
class Q_QUICK_PRIVATE_EXPORT QQuickAbstractAnimation : public QObject, public QQmlPropertyValueSource, public QQmlParserStatus
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickAbstractAnimation)

    Q_INTERFACES(QQmlParserStatus)
    Q_INTERFACES(QQmlPropertyValueSource)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool alwaysRunToEnd READ alwaysRunToEnd WRITE setAlwaysRunToEnd NOTIFY alwaysRunToEndChanged)
    Q_PROPERTY(int loops READ loops WRITE setLoops NOTIFY loopCountChanged)
    Q_CLASSINFO("DefaultMethod", "start()")

    QML_NAMED_ELEMENT(Animation)
    QML_UNCREATABLE("Animation is an abstract class")

public:
    enum ThreadingModel  {
        GuiThread,
        RenderThread,
        AnyThread
    };

    QQuickAbstractAnimation(QObject *parent=nullptr);
    ~QQuickAbstractAnimation() override;

    enum Loops { Infinite = -2 };
    Q_ENUM(Loops)

    bool isRunning() const;
    void setRunning(bool);
    bool isPaused() const;
    void setPaused(bool);
    bool alwaysRunToEnd() const;
    void setAlwaysRunToEnd(bool);

    int loops() const;
    void setLoops(int);
    int duration() const;

    int currentTime();
    void setCurrentTime(int);

    QQuickAnimationGroup *group() const;
    void setGroup(QQuickAnimationGroup *, int index = -1);

    void setDefaultTarget(const QQmlProperty &);
    void setDisableUserControl();
    void setEnableUserControl();
    bool userControlDisabled() const;
    void classBegin() override;
    void componentComplete() override;

    virtual ThreadingModel threadingModel() const;

Q_SIGNALS:
    void started();
    void stopped();
    void runningChanged(bool);
    void pausedChanged(bool);
    void alwaysRunToEndChanged(bool);
    void loopCountChanged(int);
    Q_REVISION(12) void finished();

public Q_SLOTS:
    void restart();
    void start();
    void pause();
    void resume();
    void stop();
    void complete();

protected:
    QQuickAbstractAnimation(QQuickAbstractAnimationPrivate &dd, QObject *parent);
    QAbstractAnimationJob* initInstance(QAbstractAnimationJob *animation);

public:
    enum TransitionDirection { Forward, Backward };
    virtual QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr);
    QAbstractAnimationJob* qtAnimation();

private Q_SLOTS:
    void componentFinalized();
private:
    void setTarget(const QQmlProperty &) override;
    void notifyRunningChanged(bool running);
    friend class QQuickBehavior;
    friend class QQuickBehaviorPrivate;
    friend class QQuickAnimationGroup;
};

class QQuickPauseAnimationPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickPauseAnimation : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPauseAnimation)

    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    QML_NAMED_ELEMENT(PauseAnimation)

public:
    QQuickPauseAnimation(QObject *parent=nullptr);
    ~QQuickPauseAnimation() override;

    int duration() const;
    void setDuration(int);

Q_SIGNALS:
    void durationChanged(int);

protected:
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                                          QQmlProperties &modified,
                                          TransitionDirection direction,
                                          QObject *defaultTarget = nullptr) override;
};

class QQuickScriptActionPrivate;
class QQuickScriptAction : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickScriptAction)

    Q_PROPERTY(QQmlScriptString script READ script WRITE setScript)
    Q_PROPERTY(QString scriptName READ stateChangeScriptName WRITE setStateChangeScriptName)
    QML_NAMED_ELEMENT(ScriptAction)

public:
    QQuickScriptAction(QObject *parent=nullptr);
    ~QQuickScriptAction() override;

    QQmlScriptString script() const;
    void setScript(const QQmlScriptString &);

    QString stateChangeScriptName() const;
    void setStateChangeScriptName(const QString &);

protected:
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr) override;
};

class QQuickPropertyActionPrivate;
class QQuickPropertyAction : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPropertyAction)

    Q_PROPERTY(QObject *target READ target WRITE setTargetObject NOTIFY targetChanged)
    Q_PROPERTY(QString property READ property WRITE setProperty NOTIFY propertyChanged)
    Q_PROPERTY(QString properties READ properties WRITE setProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QQmlListProperty<QObject> targets READ targets)
    Q_PROPERTY(QQmlListProperty<QObject> exclude READ exclude)
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    QML_NAMED_ELEMENT(PropertyAction)

public:
    QQuickPropertyAction(QObject *parent=nullptr);
    ~QQuickPropertyAction() override;

    QObject *target() const;
    void setTargetObject(QObject *);

    QString property() const;
    void setProperty(const QString &);

    QString properties() const;
    void setProperties(const QString &);

    QQmlListProperty<QObject> targets();
    QQmlListProperty<QObject> exclude();

    QVariant value() const;
    void setValue(const QVariant &);

Q_SIGNALS:
    void valueChanged(const QVariant &);
    void propertiesChanged(const QString &);
    void targetChanged();
    void propertyChanged();

protected:
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr) override;
};

class QQuickPropertyAnimationPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickPropertyAnimation : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPropertyAnimation)

    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(QVariant from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QVariant to READ to WRITE setTo NOTIFY toChanged)
    Q_PROPERTY(QEasingCurve easing READ easing WRITE setEasing NOTIFY easingChanged)
    Q_PROPERTY(QObject *target READ target WRITE setTargetObject NOTIFY targetChanged)
    Q_PROPERTY(QString property READ property WRITE setProperty NOTIFY propertyChanged)
    Q_PROPERTY(QString properties READ properties WRITE setProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QQmlListProperty<QObject> targets READ targets)
    Q_PROPERTY(QQmlListProperty<QObject> exclude READ exclude)
    QML_NAMED_ELEMENT(PropertyAnimation)

public:
    QQuickPropertyAnimation(QObject *parent=nullptr);
    ~QQuickPropertyAnimation() override;

    virtual int duration() const;
    virtual void setDuration(int);

    QVariant from() const;
    void setFrom(const QVariant &);

    QVariant to() const;
    void setTo(const QVariant &);

    QEasingCurve easing() const;
    void setEasing(const QEasingCurve &);

    QObject *target() const;
    void setTargetObject(QObject *);

    QString property() const;
    void setProperty(const QString &);

    QString properties() const;
    void setProperties(const QString &);

    QQmlListProperty<QObject> targets();
    QQmlListProperty<QObject> exclude();

protected:
    QQuickStateActions createTransitionActions(QQuickStateActions &actions,
                                                     QQmlProperties &modified,
                                                     QObject *defaultTarget = nullptr);

    QQuickPropertyAnimation(QQuickPropertyAnimationPrivate &dd, QObject *parent);
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr) override;
Q_SIGNALS:
    void durationChanged(int);
    void fromChanged();
    void toChanged();
    void easingChanged(const QEasingCurve &);
    void propertiesChanged(const QString &);
    void targetChanged();
    void propertyChanged();
};

class Q_QUICK_PRIVATE_EXPORT QQuickColorAnimation : public QQuickPropertyAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPropertyAnimation)
    Q_PROPERTY(QColor from READ from WRITE setFrom)
    Q_PROPERTY(QColor to READ to WRITE setTo)
    QML_NAMED_ELEMENT(ColorAnimation)

public:
    QQuickColorAnimation(QObject *parent=nullptr);
    ~QQuickColorAnimation() override;

    QColor from() const;
    void setFrom(const QColor &);

    QColor to() const;
    void setTo(const QColor &);
};

class Q_QUICK_PRIVATE_EXPORT QQuickNumberAnimation : public QQuickPropertyAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPropertyAnimation)

    Q_PROPERTY(qreal from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(qreal to READ to WRITE setTo NOTIFY toChanged)
    QML_NAMED_ELEMENT(NumberAnimation)

public:
    QQuickNumberAnimation(QObject *parent=nullptr);
    ~QQuickNumberAnimation() override;

    qreal from() const;
    void setFrom(qreal);

    qreal to() const;
    void setTo(qreal);

protected:
    QQuickNumberAnimation(QQuickPropertyAnimationPrivate &dd, QObject *parent);

private:
    void init();
};

class Q_QUICK_PRIVATE_EXPORT QQuickVector3dAnimation : public QQuickPropertyAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickPropertyAnimation)

    Q_PROPERTY(QVector3D from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QVector3D to READ to WRITE setTo NOTIFY toChanged)
    QML_NAMED_ELEMENT(Vector3dAnimation)

public:
    QQuickVector3dAnimation(QObject *parent=nullptr);
    ~QQuickVector3dAnimation() override;

    QVector3D from() const;
    void setFrom(QVector3D);

    QVector3D to() const;
    void setTo(QVector3D);
};

class QQuickRotationAnimationPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickRotationAnimation : public QQuickPropertyAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickRotationAnimation)

    Q_PROPERTY(qreal from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(qreal to READ to WRITE setTo NOTIFY toChanged)
    Q_PROPERTY(RotationDirection direction READ direction WRITE setDirection NOTIFY directionChanged)
    QML_NAMED_ELEMENT(RotationAnimation)

public:
    QQuickRotationAnimation(QObject *parent=nullptr);
    ~QQuickRotationAnimation() override;

    qreal from() const;
    void setFrom(qreal);

    qreal to() const;
    void setTo(qreal);

    enum RotationDirection { Numerical, Shortest, Clockwise, Counterclockwise };
    Q_ENUM(RotationDirection)
    RotationDirection direction() const;
    void setDirection(RotationDirection direction);

Q_SIGNALS:
    void directionChanged();
};

class QQuickAnimationGroupPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickAnimationGroup : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickAnimationGroup)

    Q_CLASSINFO("DefaultProperty", "animations")
    Q_PROPERTY(QQmlListProperty<QQuickAbstractAnimation> animations READ animations)

public:
    QQuickAnimationGroup(QObject *parent);
    ~QQuickAnimationGroup() override;

    QQmlListProperty<QQuickAbstractAnimation> animations();
    friend class QQuickAbstractAnimation;

protected:
    QQuickAnimationGroup(QQuickAnimationGroupPrivate &dd, QObject *parent);
};

class QQuickSequentialAnimation : public QQuickAnimationGroup
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickAnimationGroup)
    QML_NAMED_ELEMENT(SequentialAnimation)

public:
    QQuickSequentialAnimation(QObject *parent=nullptr);
    ~QQuickSequentialAnimation() override;

protected:
    ThreadingModel threadingModel() const override;
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr) override;
};

class Q_QUICK_PRIVATE_EXPORT QQuickParallelAnimation : public QQuickAnimationGroup
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickAnimationGroup)
    QML_NAMED_ELEMENT(ParallelAnimation)

public:
    QQuickParallelAnimation(QObject *parent=nullptr);
    ~QQuickParallelAnimation() override;

protected:
    ThreadingModel threadingModel() const override;
    QAbstractAnimationJob* transition(QQuickStateActions &actions,
                            QQmlProperties &modified,
                            TransitionDirection direction,
                            QObject *defaultTarget = nullptr) override;
};


QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickAbstractAnimation)
QML_DECLARE_TYPE(QQuickPauseAnimation)
QML_DECLARE_TYPE(QQuickScriptAction)
QML_DECLARE_TYPE(QQuickPropertyAction)
QML_DECLARE_TYPE(QQuickPropertyAnimation)
QML_DECLARE_TYPE(QQuickColorAnimation)
QML_DECLARE_TYPE(QQuickNumberAnimation)
QML_DECLARE_TYPE(QQuickSequentialAnimation)
QML_DECLARE_TYPE(QQuickParallelAnimation)
QML_DECLARE_TYPE(QQuickVector3dAnimation)
QML_DECLARE_TYPE(QQuickRotationAnimation)

#endif // QQUICKANIMATION_H
