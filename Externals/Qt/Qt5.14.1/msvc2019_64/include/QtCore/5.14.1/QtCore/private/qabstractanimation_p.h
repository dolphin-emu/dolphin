/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QABSTRACTANIMATION_P_H
#define QABSTRACTANIMATION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.
// This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qbasictimer.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qtimer.h>
#include <QtCore/qelapsedtimer.h>
#include <private/qobject_p.h>
#include <qabstractanimation.h>

QT_REQUIRE_CONFIG(animation);

QT_BEGIN_NAMESPACE

class QAnimationGroup;
class QAbstractAnimation;
class QAbstractAnimationPrivate : public QObjectPrivate
{
public:
    QAbstractAnimationPrivate()
        : state(QAbstractAnimation::Stopped),
          direction(QAbstractAnimation::Forward),
          totalCurrentTime(0),
          currentTime(0),
          loopCount(1),
          currentLoop(0),
          deleteWhenStopped(false),
          hasRegisteredTimer(false),
          isPause(false),
          isGroup(false),
          group(nullptr)
    {
    }

    virtual ~QAbstractAnimationPrivate() {}

    static QAbstractAnimationPrivate *get(QAbstractAnimation *q)
    {
        return q->d_func();
    }

    QAbstractAnimation::State state;
    QAbstractAnimation::Direction direction;
    void setState(QAbstractAnimation::State state);

    int totalCurrentTime;
    int currentTime;
    int loopCount;
    int currentLoop;

    bool deleteWhenStopped;
    bool hasRegisteredTimer;
    bool isPause;
    bool isGroup;

    QAnimationGroup *group;

private:
    Q_DECLARE_PUBLIC(QAbstractAnimation)
};


class QUnifiedTimer;
class QDefaultAnimationDriver : public QAnimationDriver
{
    Q_OBJECT
public:
    QDefaultAnimationDriver(QUnifiedTimer *timer);
    void timerEvent(QTimerEvent *e) override;

private Q_SLOTS:
    void startTimer();
    void stopTimer();

private:
    QBasicTimer m_timer;
    QUnifiedTimer *m_unified_timer;
};

class Q_CORE_EXPORT QAnimationDriverPrivate : public QObjectPrivate
{
public:
    QAnimationDriverPrivate() : running(false) {}
    QElapsedTimer timer;
    bool running;
};

class Q_CORE_EXPORT QAbstractAnimationTimer : public QObject
{
    Q_OBJECT
public:
    QAbstractAnimationTimer() : isRegistered(false), isPaused(false), pauseDuration(0) {}

    virtual void updateAnimationsTime(qint64 delta) = 0;
    virtual void restartAnimationTimer() = 0;
    virtual int runningAnimationCount() = 0;

    bool isRegistered;
    bool isPaused;
    int pauseDuration;
};

class Q_CORE_EXPORT QUnifiedTimer : public QObject
{
    Q_OBJECT
private:
    QUnifiedTimer();

public:
    static QUnifiedTimer *instance();
    static QUnifiedTimer *instance(bool create);

    static void startAnimationTimer(QAbstractAnimationTimer *timer);
    static void stopAnimationTimer(QAbstractAnimationTimer *timer);

    static void pauseAnimationTimer(QAbstractAnimationTimer *timer, int duration);
    static void resumeAnimationTimer(QAbstractAnimationTimer *timer);

    //defines the timing interval. Default is DEFAULT_TIMER_INTERVAL
    void setTimingInterval(int interval);

    /*
       this allows to have a consistent timer interval at each tick from the timer
       not taking the real time that passed into account.
    */
    void setConsistentTiming(bool consistent) { consistentTiming = consistent; }

    //these facilitate fine-tuning of complex animations
    void setSlowModeEnabled(bool enabled) { slowMode = enabled; }
    void setSlowdownFactor(qreal factor) { slowdownFactor = factor; }

    void installAnimationDriver(QAnimationDriver *driver);
    void uninstallAnimationDriver(QAnimationDriver *driver);
    bool canUninstallAnimationDriver(QAnimationDriver *driver);

    void restart();
    void maybeUpdateAnimationsToCurrentTime();
    void updateAnimationTimers(qint64 currentTick);

    //useful for profiling/debugging
    int runningAnimationCount();
    void registerProfilerCallback(void (*cb)(qint64));

    void startAnimationDriver();
    void stopAnimationDriver();
    qint64 elapsed() const;

protected:
    void timerEvent(QTimerEvent *) override;

private Q_SLOTS:
    void startTimers();
    void stopTimer();

private:
    friend class QDefaultAnimationDriver;
    friend class QAnimationDriver;

    QAnimationDriver *driver;
    QDefaultAnimationDriver defaultDriver;

    QBasicTimer pauseTimer;

    QElapsedTimer time;

    qint64 lastTick;
    int timingInterval;
    int currentAnimationIdx;
    bool insideTick;
    bool insideRestart;
    bool consistentTiming;
    bool slowMode;
    bool startTimersPending;
    bool stopTimerPending;

    // This factor will be used to divide the DEFAULT_TIMER_INTERVAL at each tick
    // when slowMode is enabled. Setting it to 0 or higher than DEFAULT_TIMER_INTERVAL (16)
    // stops all animations.
    qreal slowdownFactor;

    QList<QAbstractAnimationTimer*> animationTimers, animationTimersToStart;
    QList<QAbstractAnimationTimer*> pausedAnimationTimers;

    void localRestart();
    int closestPausedAnimationTimerTimeToFinish();

    void (*profilerCallback)(qint64);

    qint64 driverStartTime; // The time the animation driver was started
    qint64 temporalDrift; // The delta between animation driver time and wall time.
};

class QAnimationTimer : public QAbstractAnimationTimer
{
    Q_OBJECT
private:
    QAnimationTimer();

public:
    static QAnimationTimer *instance();
    static QAnimationTimer *instance(bool create);

    static void registerAnimation(QAbstractAnimation *animation, bool isTopLevel);
    static void unregisterAnimation(QAbstractAnimation *animation);

    /*
        this is used for updating the currentTime of all animations in case the pause
        timer is active or, otherwise, only of the animation passed as parameter.
    */
    static void ensureTimerUpdate();

    /*
        this will evaluate the need of restarting the pause timer in case there is still
        some pause animations running.
    */
    static void updateAnimationTimer();

    void restartAnimationTimer() override;
    void updateAnimationsTime(qint64 delta) override;

    //useful for profiling/debugging
    int runningAnimationCount() override { return animations.count(); }

private Q_SLOTS:
    void startAnimations();
    void stopTimer();

private:
    qint64 lastTick;
    int currentAnimationIdx;
    bool insideTick;
    bool startAnimationPending;
    bool stopTimerPending;

    QList<QAbstractAnimation*> animations, animationsToStart;

    // this is the count of running animations that are not a group neither a pause animation
    int runningLeafAnimations;
    QList<QAbstractAnimation*> runningPauseAnimations;

    void registerRunningAnimation(QAbstractAnimation *animation);
    void unregisterRunningAnimation(QAbstractAnimation *animation);

    int closestPauseAnimationTimeToFinish();
};

QT_END_NAMESPACE

#endif //QABSTRACTANIMATION_P_H
