/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Gunnar Sletta <gunnar@sletta.org>
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

#ifndef QQUICKANIMATORJOB_P_H
#define QQUICKANIMATORJOB_P_H

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

#include <private/qabstractanimationjob_p.h>
#include <private/qquickanimator_p.h>
#include <private/qtquickglobal_p.h>

#include <QtQuick/qquickitem.h>

#include <QtCore/qeasingcurve.h>

QT_BEGIN_NAMESPACE

class QQuickAnimator;
class QQuickWindow;
class QQuickItem;
class QQuickAbstractAnimation;

class QQuickAnimatorController;
class QQuickAnimatorProxyJobPrivate;
class QQuickOpenGLShaderEffectNode;

class QSGOpacityNode;

class Q_QUICK_PRIVATE_EXPORT QQuickAnimatorProxyJob : public QObject, public QAbstractAnimationJob
{
    Q_OBJECT

public:
    QQuickAnimatorProxyJob(QAbstractAnimationJob *job, QObject *item);
    ~QQuickAnimatorProxyJob();

    int duration() const override { return m_duration; }

    const QSharedPointer<QAbstractAnimationJob> &job() const { return m_job; }

protected:
    void updateCurrentTime(int) override;
    void updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State oldState) override;
    void debugAnimation(QDebug d) const override;

public Q_SLOTS:
    void windowChanged(QQuickWindow *window);
    void sceneGraphInitialized();

private:
    void syncBackCurrentValues();
    void readyToAnimate();
    void setWindow(QQuickWindow *window);
    static QObject *findAnimationContext(QQuickAbstractAnimation *);

    QPointer<QQuickAnimatorController> m_controller;
    QQuickAbstractAnimation *m_animation;
    QSharedPointer<QAbstractAnimationJob> m_job;
    int m_duration;

    enum InternalState {
        State_Starting, // Used when it should be running, but no we're still missing the controller.
        State_Running,
        State_Paused,
        State_Stopped
    };

    InternalState m_internalState;
};

class Q_QUICK_PRIVATE_EXPORT QQuickAnimatorJob : public QAbstractAnimationJob
{
public:
    virtual void setTarget(QQuickItem *target);
    QQuickItem *target() const { return m_target; }

    void setFrom(qreal from) { m_from = from; }
    qreal from() const { return m_from; }

    void setTo(qreal to) { m_to = to; }
    qreal to() const { return m_to; }

    void setDuration(int duration) { m_duration = duration; }
    int duration() const override { return m_duration; }

    QEasingCurve easingCurve() const { return m_easing; }
    void setEasingCurve(const QEasingCurve &curve) { m_easing = curve; }

    // Initialize is called on the GUI thread just before it is started
    // and taken over on the render thread.
    virtual void initialize(QQuickAnimatorController *controller);

    // Called on the render thread during SG shutdown.
    virtual void invalidate() = 0;

    // Called on the GUI thread after a complete render thread animation job
    // has been completed to write back a given animator's result to the
    // source item.
    virtual void writeBack() = 0;

    // Called before the SG sync on the render thread. The GUI thread is
    // locked during this call.
    virtual void preSync() { }

    // Called after the SG sync on the render thread. The GUI thread is
    // locked during this call.
    virtual void postSync() { }

    // Called after animations have ticked on the render thread. No locks are
    // held at this time, so synchronization needs to be taken into account
    // if applicable.
    virtual void commit() { }

    bool isTransform() const { return m_isTransform; }
    bool isUniform() const { return m_isUniform; }

    qreal value() const;

    QQuickAnimatorController *controller() const { return m_controller; }

protected:
    QQuickAnimatorJob();
    void debugAnimation(QDebug d) const override;

    qreal progress(int time) const;

    QPointer<QQuickItem> m_target;
    QQuickAnimatorController *m_controller;

    qreal m_from;
    qreal m_to;
    qreal m_value;

    QEasingCurve m_easing;

    int m_duration;

    uint m_isTransform : 1;
    uint m_isUniform : 1;
};

class QQuickTransformAnimatorJob : public QQuickAnimatorJob
{
public:

    struct Helper
    {
        Helper()
            : ref(1)
            , node(nullptr)
            , ox(0)
            , oy(0)
            , dx(0)
            , dy(0)
            , scale(1)
            , rotation(0)
            , wasSynced(false)
            , wasChanged(false)
        {
        }

        void sync();
        void commit();

        int ref;
        QQuickItem *item;
        QSGTransformNode *node;

        // Origin
        float ox;
        float oy;

        float dx;
        float dy;
        float scale;
        float rotation;

        uint wasSynced : 1;
        uint wasChanged : 1;
    };

    ~QQuickTransformAnimatorJob();

    void commit() override;
    void preSync() override;

    void setTarget(QQuickItem *item) override;

protected:
    QQuickTransformAnimatorJob();
    void invalidate() override;

    Helper *m_helper;
};

class Q_QUICK_PRIVATE_EXPORT QQuickScaleAnimatorJob : public QQuickTransformAnimatorJob
{
public:
    void updateCurrentTime(int time) override;
    void writeBack() override;
};

class Q_QUICK_PRIVATE_EXPORT QQuickXAnimatorJob : public QQuickTransformAnimatorJob
{
public:
    void updateCurrentTime(int time) override;
    void writeBack() override;
};

class Q_QUICK_PRIVATE_EXPORT QQuickYAnimatorJob : public QQuickTransformAnimatorJob
{
public:
    void updateCurrentTime(int time) override;
    void writeBack() override;
};

class Q_QUICK_PRIVATE_EXPORT QQuickRotationAnimatorJob : public QQuickTransformAnimatorJob
{
public:
    QQuickRotationAnimatorJob();

    void updateCurrentTime(int time) override;
    void writeBack() override;

    void setDirection(QQuickRotationAnimator::RotationDirection direction) { m_direction = direction; }
    QQuickRotationAnimator::RotationDirection direction() const { return m_direction; }

private:
    QQuickRotationAnimator::RotationDirection m_direction;
};

class Q_QUICK_PRIVATE_EXPORT QQuickOpacityAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickOpacityAnimatorJob();

    void invalidate() override;
    void updateCurrentTime(int time) override;
    void writeBack() override;
    void postSync() override;

private:
    QSGOpacityNode *m_opacityNode;
};
#if QT_CONFIG(opengl)
class Q_QUICK_PRIVATE_EXPORT QQuickUniformAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickUniformAnimatorJob();

    void setTarget(QQuickItem *target) override;

    void setUniform(const QByteArray &uniform) { m_uniform = uniform; }
    QByteArray uniform() const { return m_uniform; }

    void postSync() override;

    void updateCurrentTime(int time) override;
    void writeBack() override;

    void invalidate() override;

private:
    QByteArray m_uniform;
    QQuickOpenGLShaderEffectNode *m_node;

    int m_uniformIndex : 8;
    int m_uniformType : 8;
};
#endif

QT_END_NAMESPACE

#endif // QQUICKANIMATORJOB_P_H
