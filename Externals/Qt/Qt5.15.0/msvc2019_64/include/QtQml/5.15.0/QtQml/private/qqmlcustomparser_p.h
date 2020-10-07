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

#ifndef QQMLCUSTOMPARSER_H
#define QQMLCUSTOMPARSER_H

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

#include "qqmlerror.h"
#include "qqmlbinding_p.h"
#include <private/qv4compileddata_p.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qxmlstream.h>

QT_BEGIN_NAMESPACE

class QQmlPropertyValidator;
class QQmlEnginePrivate;

class Q_QML_PRIVATE_EXPORT QQmlCustomParser
{
public:
    enum Flag {
        NoFlag                    = 0x00000000,
        AcceptsAttachedProperties = 0x00000001,
        AcceptsSignalHandlers     = 0x00000002
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QQmlCustomParser() : engine(nullptr), validator(nullptr), m_flags(NoFlag) {}
    QQmlCustomParser(Flags f) : engine(nullptr), validator(nullptr), m_flags(f) {}
    virtual ~QQmlCustomParser() {}

    void clearErrors();
    Flags flags() const { return m_flags; }

    virtual void verifyBindings(const QQmlRefPointer<QV4::ExecutableCompilationUnit> &, const QList<const QV4::CompiledData::Binding *> &) = 0;
    virtual void applyBindings(QObject *, const QQmlRefPointer<QV4::ExecutableCompilationUnit> &, const QList<const QV4::CompiledData::Binding *> &) = 0;

    QVector<QQmlError> errors() const { return exceptions; }

protected:
    void error(const QV4::CompiledData::Binding *binding, const QString& description)
    { error(binding->location, description); }
    void error(const QV4::CompiledData::Object *object, const QString& description)
    { error(object->location, description); }
    void error(const QV4::CompiledData::Location &location, const QString& description);

    int evaluateEnum(const QByteArray&, bool *ok) const;

    const QMetaObject *resolveType(const QString&) const;

private:
    QVector<QQmlError> exceptions;
    QQmlEnginePrivate *engine;
    const QQmlPropertyValidator *validator;
    Flags m_flags;
    QBiPointer<const QQmlImports, QQmlTypeNameCache> imports;
    friend class QQmlPropertyValidator;
    friend class QQmlObjectCreator;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlCustomParser::Flags)

QT_END_NAMESPACE

#endif
