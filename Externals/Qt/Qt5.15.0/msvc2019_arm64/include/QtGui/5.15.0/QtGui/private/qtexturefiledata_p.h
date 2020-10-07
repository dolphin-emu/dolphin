/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QTEXTUREFILEDATA_P_H
#define QTEXTUREFILEDATA_P_H

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

#include <QtGui/qtguiglobal.h>
#include <QSharedDataPointer>
#include <QLoggingCategory>
#include <QDebug>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQtGuiTextureIO)

class QTextureFileDataPrivate;

class Q_GUI_EXPORT QTextureFileData
{
public:
    QTextureFileData();
    QTextureFileData(const QTextureFileData &other);
    QTextureFileData &operator=(const QTextureFileData &other);
    ~QTextureFileData();

    bool isNull() const;
    bool isValid() const;

    void clear();

    QByteArray data() const;
    void setData(const QByteArray &data);

    int dataOffset(int level = 0) const;
    void setDataOffset(int offset, int level = 0);

    int dataLength(int level = 0) const;
    void setDataLength(int length, int level = 0);

    int numLevels() const;
    void setNumLevels(int num);

    QSize size() const;
    void setSize(const QSize &size);

    quint32 glFormat() const;
    void setGLFormat(quint32 format);

    quint32 glInternalFormat() const;
    void setGLInternalFormat(quint32 format);

    quint32 glBaseInternalFormat() const;
    void setGLBaseInternalFormat(quint32 format);

    QByteArray logName() const;
    void setLogName(const QByteArray &name);

private:
    QSharedDataPointer<QTextureFileDataPrivate> d;
};

Q_DECLARE_TYPEINFO(QTextureFileData, Q_MOVABLE_TYPE);

Q_GUI_EXPORT QDebug operator<<(QDebug dbg, const QTextureFileData &d);

QT_END_NAMESPACE

#endif // QABSTRACTLAYOUTSTYLEINFO_P_H
