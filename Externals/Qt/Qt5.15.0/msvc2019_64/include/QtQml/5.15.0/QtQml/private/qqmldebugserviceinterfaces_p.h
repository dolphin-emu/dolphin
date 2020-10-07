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

#ifndef QQMLDEBUGSERVICEINTERFACES_P_H
#define QQMLDEBUGSERVICEINTERFACES_P_H

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

#include <QtCore/qstring.h>
#include <private/qtqmlglobal_p.h>
#if QT_CONFIG(qml_debug)
#include <private/qqmldebugservice_p.h>
#endif
#include <private/qqmldebugstatesdelegate_p.h>
#include <private/qqmlboundsignal_p.h>

#include <limits>

QT_BEGIN_NAMESPACE

class QWindow;
class QQuickWindow;
class QQmlTranslationBinding;

#if !QT_CONFIG(qml_debug)

class QV4DebugService
{
public:
    void signalEmitted(const QString &) {}
};

class QQmlProfilerService
{
public:
    void startProfiling(QJSEngine *engine, quint64 features = std::numeric_limits<quint64>::max())
    {
        Q_UNUSED(engine);
        Q_UNUSED(features);
    }

    void stopProfiling(QJSEngine *) {}
};

class QQmlEngineDebugService
{
public:
    void objectCreated(QJSEngine *, QObject *) {}
    virtual void setStatesDelegate(QQmlDebugStatesDelegate *) {}
};

class QQmlInspectorService {
public:
    void addWindow(QQuickWindow *) {}
    void setParentWindow(QQuickWindow *, QWindow *) {}
    void removeWindow(QQuickWindow *) {}
};

class QDebugMessageService {};
class QQmlEngineControlService {};
class QQmlNativeDebugService {};
class QQmlDebugTranslationService {
public:
    virtual QString foundElidedText(QObject *, const QString &, const QString &) {return {};}
    virtual void foundTranslationBinding(QQmlTranslationBinding *, QObject *, QQmlContextData *) {}
};

#else

class Q_QML_PRIVATE_EXPORT QV4DebugService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual void signalEmitted(const QString &signal) = 0;

protected:
    friend class QQmlDebugConnector;

    QV4DebugService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}
};

class QQmlAbstractProfilerAdapter;
class Q_QML_PRIVATE_EXPORT QQmlProfilerService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual void addGlobalProfiler(QQmlAbstractProfilerAdapter *profiler) = 0;
    virtual void removeGlobalProfiler(QQmlAbstractProfilerAdapter *profiler) = 0;

    virtual void startProfiling(QJSEngine *engine,
                                quint64 features = std::numeric_limits<quint64>::max()) = 0;
    virtual void stopProfiling(QJSEngine *engine) = 0;

    virtual void dataReady(QQmlAbstractProfilerAdapter *profiler) = 0;

protected:
    friend class QQmlDebugConnector;

    QQmlProfilerService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}
};

class Q_QML_PRIVATE_EXPORT QQmlEngineDebugService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual void objectCreated(QJSEngine *engine, QObject *object) = 0;
    virtual void setStatesDelegate(QQmlDebugStatesDelegate *) = 0;

protected:
    friend class QQmlDebugConnector;

    QQmlEngineDebugService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}

    QQmlBoundSignal *nextSignal(QQmlBoundSignal *prev) { return prev->m_nextSignal; }
};

class Q_QML_PRIVATE_EXPORT QQmlDebugTranslationService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual QString foundElidedText(QObject *qQuickTextObject, const QString &layoutText, const QString &elideText) = 0;
    virtual void foundTranslationBinding(QQmlTranslationBinding *binding, QObject *scopeObject, QQmlContextData *contextData) = 0;
protected:
    friend class QQmlDebugConnector;

    QQmlDebugTranslationService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}

};

class Q_QML_PRIVATE_EXPORT QQmlInspectorService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual void addWindow(QQuickWindow *) = 0;
    virtual void setParentWindow(QQuickWindow *, QWindow *) = 0;
    virtual void removeWindow(QQuickWindow *) = 0;

protected:
    friend class QQmlDebugConnector;

    QQmlInspectorService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}
};

class Q_QML_PRIVATE_EXPORT QDebugMessageService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

    virtual void synchronizeTime(const QElapsedTimer &otherTimer) = 0;

protected:
    friend class QQmlDebugConnector;

    QDebugMessageService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}
};

class Q_QML_PRIVATE_EXPORT QQmlEngineControlService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

protected:
    friend class QQmlDebugConnector;

    QQmlEngineControlService(float version, QObject *parent = nullptr) :
        QQmlDebugService(s_key, version, parent) {}

};

class Q_QML_PRIVATE_EXPORT QQmlNativeDebugService : public QQmlDebugService
{
    Q_OBJECT
public:
    static const QString s_key;

protected:
    friend class QQmlDebugConnector;

    QQmlNativeDebugService(float version, QObject *parent = nullptr)
        : QQmlDebugService(s_key, version,  parent) {}
};

#endif

QT_END_NAMESPACE

#endif // QQMLDEBUGSERVICEINTERFACES_P_H

