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

#ifndef QQUICKANIMATOR_P_H
#define QQUICKANIMATOR_P_H

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

QT_BEGIN_NAMESPACE

class QQuickItem;

class QQuickAnimatorJob;
class QQuickAnimatorPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickAnimator : public QQuickAbstractAnimation
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickAnimator)
    Q_PROPERTY(QQuickItem *target READ targetItem WRITE setTargetItem NOTIFY targetItemChanged)
    Q_PROPERTY(QEasingCurve easing READ easing WRITE setEasing NOTIFY easingChanged)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(qreal to READ to WRITE setTo NOTIFY toChanged)
    Q_PROPERTY(qreal from READ from WRITE setFrom NOTIFY fromChanged)

    QML_NAMED_ELEMENT(Animator)
    QML_ADDED_IN_MINOR_VERSION(2)
    QML_UNCREATABLE("Animator is an abstract class")

public:
    QQuickItem *targetItem() const;
    void setTargetItem(QQuickItem *target);

    int duration() const;
    void setDuration(int duration);

    QEasingCurve easing() const;
    void setEasing(const QEasingCurve & easing);

    qreal to() const;
    void setTo(qreal to);

    qreal from() const;
    void setFrom(qreal from);

protected:
    ThreadingModel threadingModel() const override { return RenderThread; }
    virtual QQuickAnimatorJob *createJob() const = 0;
    virtual QString propertyName() const = 0;
    QAbstractAnimationJob *transition(QQuickStateActions &actions,
                                      QQmlProperties &modified,
                                      TransitionDirection,
                                      QObject *) override;

    QQuickAnimator(QQuickAnimatorPrivate &dd, QObject *parent = nullptr);
    QQuickAnimator(QObject *parent = nullptr);

Q_SIGNALS:
    void targetItemChanged(QQuickItem *);
    void durationChanged(int duration);
    void easingChanged(const QEasingCurve &curve);
    void toChanged(qreal to);
    void fromChanged(qreal from);
};

class QQuickScaleAnimatorPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickScaleAnimator : public QQuickAnimator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScaleAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)
public:
    QQuickScaleAnimator(QObject *parent = nullptr);
protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override { return QStringLiteral("scale"); }
};

class Q_QUICK_PRIVATE_EXPORT QQuickXAnimator : public QQuickAnimator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(XAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)
public:
    QQuickXAnimator(QObject *parent = nullptr);
protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override { return QStringLiteral("x"); }
};

class Q_QUICK_PRIVATE_EXPORT QQuickYAnimator : public QQuickAnimator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(YAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)
public:
    QQuickYAnimator(QObject *parent = nullptr);
protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override { return QStringLiteral("y"); }
};

class Q_QUICK_PRIVATE_EXPORT QQuickOpacityAnimator : public QQuickAnimator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(OpacityAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)
public:
    QQuickOpacityAnimator(QObject *parent = nullptr);
protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override { return QStringLiteral("opacity"); }
};

class QQuickRotationAnimatorPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickRotationAnimator : public QQuickAnimator
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickRotationAnimator)
    Q_PROPERTY(RotationDirection direction READ direction WRITE setDirection NOTIFY directionChanged)
    QML_NAMED_ELEMENT(RotationAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)

public:
    enum RotationDirection { Numerical, Shortest, Clockwise, Counterclockwise };
    Q_ENUM(RotationDirection)

    QQuickRotationAnimator(QObject *parent = nullptr);

    void setDirection(RotationDirection dir);
    RotationDirection direction() const;

Q_SIGNALS:
    void directionChanged(RotationDirection dir);

protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override { return QStringLiteral("rotation"); }
};

#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
class QQuickUniformAnimatorPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickUniformAnimator : public QQuickAnimator
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickUniformAnimator)
    Q_PROPERTY(QString uniform READ uniform WRITE setUniform NOTIFY uniformChanged)
    QML_NAMED_ELEMENT(UniformAnimator)
    QML_ADDED_IN_MINOR_VERSION(2)

public:
    QQuickUniformAnimator(QObject *parent = nullptr);

    QString uniform() const;
    void setUniform(const QString &);

Q_SIGNALS:
    void uniformChanged(const QString &);

protected:
    QQuickAnimatorJob *createJob() const override;
    QString propertyName() const override;
};
#endif

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickAnimator)
QML_DECLARE_TYPE(QQuickXAnimator)
QML_DECLARE_TYPE(QQuickYAnimator)
QML_DECLARE_TYPE(QQuickScaleAnimator)
QML_DECLARE_TYPE(QQuickRotationAnimator)
QML_DECLARE_TYPE(QQuickOpacityAnimator)
#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
QML_DECLARE_TYPE(QQuickUniformAnimator)
#endif
#endif // QQUICKANIMATOR_P_H
