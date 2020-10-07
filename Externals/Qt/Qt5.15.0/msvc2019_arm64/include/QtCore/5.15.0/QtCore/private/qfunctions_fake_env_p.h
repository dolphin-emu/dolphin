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

#ifndef QFUNCTIONS_FAKE_ENV_P_H
#define QFUNCTIONS_FAKE_ENV_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qbytearray.h"
#include "qvector.h"

QT_BEGIN_NAMESPACE

// Environment ------------------------------------------------------
struct Variable {
    Variable() { }

    Variable(const QByteArray &name, const QByteArray &value)
        : name(name), value(value) { }

    QByteArray name;
    QByteArray value;
};

Q_DECLARE_TYPEINFO(Variable, Q_MOVABLE_TYPE);

struct NameEquals {
    typedef bool result_type;
    const char *name;
    explicit NameEquals(const char *name) noexcept : name(name) {}
    result_type operator()(const Variable &other) const noexcept
    { return qstrcmp(other.name, name) == 0; }
};

#ifndef Q_CLANG_QDOC
Q_GLOBAL_STATIC(QVector<Variable>, qt_app_environment)
#endif

errno_t qt_fake_getenv_s(size_t *sizeNeeded, char *buffer, size_t bufferSize, const char *varName)
{
    if (!sizeNeeded)
        return EINVAL;

    QVector<Variable>::const_iterator end = qt_app_environment->constEnd();
    QVector<Variable>::const_iterator iterator = std::find_if(qt_app_environment->constBegin(),
                                                              end,
                                                              NameEquals(varName));
    if (iterator == end) {
        if (buffer)
            buffer[0] = '\0';
        return ENOENT;
    }

    const int size = iterator->value.size() + 1;
    if (bufferSize < size_t(size)) {
        *sizeNeeded = size;
        return ERANGE;
    }

    qstrcpy(buffer, iterator->value.constData());
    return 0;
}

errno_t qt_fake__putenv_s(const char *varName, const char *value)
{
    QVector<Variable>::iterator end = qt_app_environment->end();
    QVector<Variable>::iterator iterator = std::find_if(qt_app_environment->begin(),
                                                        end,
                                                        NameEquals(varName));
    if (!value || !*value) {
        if (iterator != end)
            qt_app_environment->erase(iterator);
    } else {
        if (iterator == end)
            qt_app_environment->append(Variable(varName, value));
        else
            iterator->value = value;
    }

    return 0;
}

QT_END_NAMESPACE

#endif // QFUNCTIONS_FAKE_ENV_P_H
