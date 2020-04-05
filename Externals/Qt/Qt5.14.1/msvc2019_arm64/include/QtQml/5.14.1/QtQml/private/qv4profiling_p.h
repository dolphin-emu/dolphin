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

#ifndef QV4PROFILING_H
#define QV4PROFILING_H

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

#include "qv4global_p.h"
#include "qv4engine_p.h"
#include "qv4function_p.h"

#include <QElapsedTimer>

#if !QT_CONFIG(qml_debug)

#define Q_V4_PROFILE_ALLOC(engine, size, type) (!engine)
#define Q_V4_PROFILE_DEALLOC(engine, size, type) (!engine)

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Profiling {
class Profiler {};
class FunctionCallProfiler {
public:
    FunctionCallProfiler(ExecutionEngine *, Function *) {}
};
}
}

QT_END_NAMESPACE

#else

#define Q_V4_PROFILE_ALLOC(engine, size, type)\
    (engine->profiler() &&\
            (engine->profiler()->featuresEnabled & (1 << Profiling::FeatureMemoryAllocation)) ?\
        engine->profiler()->trackAlloc(size, type) : false)

#define Q_V4_PROFILE_DEALLOC(engine, size, type) \
    (engine->profiler() &&\
            (engine->profiler()->featuresEnabled & (1 << Profiling::FeatureMemoryAllocation)) ?\
        engine->profiler()->trackDealloc(size, type) : false)

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Profiling {

enum Features {
    FeatureFunctionCall,
    FeatureMemoryAllocation
};

enum MemoryType {
    HeapPage,
    LargeItem,
    SmallItem
};

struct FunctionCallProperties {
    qint64 start;
    qint64 end;
    quintptr id;
};

struct FunctionLocation {
    FunctionLocation(const QString &name = QString(), const QString &file = QString(),
                     int line = -1, int column = -1) :
        name(name), file(file), line(line), column(column)
    {}

    bool isValid()
    {
        return !name.isEmpty();
    }

    QString name;
    QString file;
    int line;
    int column;
};

typedef QHash<quintptr, QV4::Profiling::FunctionLocation> FunctionLocationHash;

struct MemoryAllocationProperties {
    qint64 timestamp;
    qint64 size;
    MemoryType type;
};

class FunctionCall {
public:

    FunctionCall() : m_function(nullptr), m_start(0), m_end(0)
    { Q_ASSERT_X(false, Q_FUNC_INFO, "Cannot construct a function call without function"); }

    FunctionCall(Function *function, qint64 start, qint64 end) :
        m_function(function), m_start(start), m_end(end)
    { m_function->executableCompilationUnit()->addref(); }

    FunctionCall(const FunctionCall &other) :
        m_function(other.m_function), m_start(other.m_start), m_end(other.m_end)
    { m_function->executableCompilationUnit()->addref(); }

    ~FunctionCall()
    { m_function->executableCompilationUnit()->release(); }

    FunctionCall &operator=(const FunctionCall &other) {
        if (&other != this) {
            other.m_function->executableCompilationUnit()->addref();
            m_function->executableCompilationUnit()->release();
            m_function = other.m_function;
            m_start = other.m_start;
            m_end = other.m_end;
        }
        return *this;
    }

    Function *function() const
    {
        return m_function;
    }

    FunctionLocation resolveLocation() const;
    FunctionCallProperties properties() const;

private:
    friend bool operator<(const FunctionCall &call1, const FunctionCall &call2);

    Function *m_function;
    qint64 m_start;
    qint64 m_end;
};

class Q_QML_EXPORT Profiler : public QObject {
    Q_OBJECT
public:
    struct SentMarker {
        SentMarker() : m_function(nullptr) {}

        SentMarker(const SentMarker &other) : m_function(other.m_function)
        {
            if (m_function)
                m_function->executableCompilationUnit()->addref();
        }

        ~SentMarker()
        {
            if (m_function)
                m_function->executableCompilationUnit()->release();
        }

        SentMarker &operator=(const SentMarker &other)
        {
            if (&other != this) {
                if (m_function)
                    m_function->executableCompilationUnit()->release();
                m_function = other.m_function;
                m_function->executableCompilationUnit()->addref();
            }
            return *this;
        }

        void setFunction(Function *function)
        {
            Q_ASSERT(m_function == nullptr);
            m_function = function;
            m_function->executableCompilationUnit()->addref();
        }

        bool isValid() const
        { return m_function != nullptr; }

    private:
        Function *m_function;
    };

    Profiler(QV4::ExecutionEngine *engine);

    bool trackAlloc(size_t size, MemoryType type)
    {
        if (size) {
            MemoryAllocationProperties allocation = {m_timer.nsecsElapsed(), (qint64)size, type};
            m_memory_data.append(allocation);
            return true;
        } else {
            return false;
        }
    }

    bool trackDealloc(size_t size, MemoryType type)
    {
        if (size) {
            MemoryAllocationProperties allocation = {m_timer.nsecsElapsed(), -(qint64)size, type};
            m_memory_data.append(allocation);
            return true;
        } else {
            return false;
        }
    }

    quint64 featuresEnabled;

    void stopProfiling();
    void startProfiling(quint64 features);
    void reportData();
    void setTimer(const QElapsedTimer &timer) { m_timer = timer; }

signals:
    void dataReady(const QV4::Profiling::FunctionLocationHash &,
                   const QVector<QV4::Profiling::FunctionCallProperties> &,
                   const QVector<QV4::Profiling::MemoryAllocationProperties> &);

private:
    QV4::ExecutionEngine *m_engine;
    QElapsedTimer m_timer;
    QVector<FunctionCall> m_data;
    QVector<MemoryAllocationProperties> m_memory_data;
    QHash<quintptr, SentMarker> m_sentLocations;

    friend class FunctionCallProfiler;
};

class FunctionCallProfiler {
    Q_DISABLE_COPY(FunctionCallProfiler)
public:

    // It's enough to ref() the function in the destructor as it will probably not disappear while
    // it's executing ...
    FunctionCallProfiler(ExecutionEngine *engine, Function *f)
        : profiler(nullptr)
    {
        Profiler *p = engine->profiler();
        if (Q_UNLIKELY(p) && (p->featuresEnabled & (1 << Profiling::FeatureFunctionCall))) {
            profiler = p;
            function = f;
            startTime = profiler->m_timer.nsecsElapsed();
        }
    }

    ~FunctionCallProfiler()
    {
        if (profiler)
            profiler->m_data.append(FunctionCall(function, startTime, profiler->m_timer.nsecsElapsed()));
    }

    Profiler *profiler;
    Function *function;
    qint64 startTime;
};


} // namespace Profiling
} // namespace QV4

Q_DECLARE_TYPEINFO(QV4::Profiling::MemoryAllocationProperties, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QV4::Profiling::FunctionCallProperties, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QV4::Profiling::FunctionCall, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QV4::Profiling::FunctionLocation, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QV4::Profiling::Profiler::SentMarker, Q_MOVABLE_TYPE);

QT_END_NAMESPACE
Q_DECLARE_METATYPE(QV4::Profiling::FunctionLocationHash)
Q_DECLARE_METATYPE(QVector<QV4::Profiling::FunctionCallProperties>)
Q_DECLARE_METATYPE(QVector<QV4::Profiling::MemoryAllocationProperties>)

#endif // QT_CONFIG(qml_debug)

#endif // QV4PROFILING_H
