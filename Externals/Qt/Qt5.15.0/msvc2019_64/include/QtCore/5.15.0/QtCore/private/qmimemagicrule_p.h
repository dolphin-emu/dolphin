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

#ifndef QMIMEMAGICRULE_P_H
#define QMIMEMAGICRULE_P_H

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

#include <QtCore/private/qglobal_p.h>

QT_REQUIRE_CONFIG(mimetype);

#include <QtCore/qbytearray.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE

class QMimeMagicRule
{
public:
    enum Type { Invalid = 0, String, Host16, Host32, Big16, Big32, Little16, Little32, Byte };

    QMimeMagicRule(const QString &typeStr, const QByteArray &value, const QString &offsets,
                   const QByteArray &mask, QString *errorString);

    void swap(QMimeMagicRule &other) noexcept
    {
        qSwap(m_type,          other.m_type);
        qSwap(m_value,         other.m_value);
        qSwap(m_startPos,      other.m_startPos);
        qSwap(m_endPos,        other.m_endPos);
        qSwap(m_mask,          other.m_mask);
        qSwap(m_pattern,       other.m_pattern);
        qSwap(m_number,        other.m_number);
        qSwap(m_numberMask,    other.m_numberMask);
        qSwap(m_matchFunction, other.m_matchFunction);
    }

    bool operator==(const QMimeMagicRule &other) const;

    Type type() const { return m_type; }
    QByteArray value() const { return m_value; }
    int startPos() const { return m_startPos; }
    int endPos() const { return m_endPos; }
    QByteArray mask() const;

    bool isValid() const { return m_matchFunction != nullptr; }

    bool matches(const QByteArray &data) const;

    QList<QMimeMagicRule> m_subMatches;

    static Type type(const QByteArray &type);
    static QByteArray typeName(Type type);

    static bool matchSubstring(const char *dataPtr, int dataSize, int rangeStart, int rangeLength, int valueLength, const char *valueData, const char *mask);

private:
    Type m_type;
    QByteArray m_value;
    int m_startPos;
    int m_endPos;
    QByteArray m_mask;

    QByteArray m_pattern;
    quint32 m_number;
    quint32 m_numberMask;

    typedef bool (QMimeMagicRule::*MatchFunction)(const QByteArray &data) const;
    MatchFunction m_matchFunction;

private:
    // match functions
    bool matchString(const QByteArray &data) const;
    template <typename T>
    bool matchNumber(const QByteArray &data) const;
};
Q_DECLARE_SHARED(QMimeMagicRule)

QT_END_NAMESPACE

#endif // QMIMEMAGICRULE_H
