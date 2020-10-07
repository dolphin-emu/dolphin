/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QABSTRACTFILEICONENGINE_P_H
#define QABSTRACTFILEICONENGINE_P_H

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

#include <QtCore/qfileinfo.h>
#include <private/qicon_p.h>
#include <qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

class QAbstractFileIconEngine : public QPixmapIconEngine
{
public:
    explicit QAbstractFileIconEngine(const QFileInfo &info, QPlatformTheme::IconOptions opts)
        : QPixmapIconEngine(), m_fileInfo(info), m_options(opts) {}

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State) override;
    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override;

    QFileInfo fileInfo() const { return m_fileInfo; }
    QPlatformTheme::IconOptions options() const { return m_options; }

    // Helper to convert a sequence of ints to a list of QSize
    template <class It> static QList<QSize> toSizeList(It i1, It i2);

protected:
    virtual QPixmap filePixmap(const QSize &size, QIcon::Mode mode, QIcon::State) = 0;
    virtual QString cacheKey() const;

private:
    const QFileInfo m_fileInfo;
    const QPlatformTheme::IconOptions m_options;
};

template <class It>
inline QList<QSize> QAbstractFileIconEngine::toSizeList(It i1, It i2)
{
    QList<QSize> result;
    result.reserve(int(i2 - i1));
    for ( ; i1 != i2; ++i1)
        result.append(QSize(*i1, *i1));
    return result;
}

QT_END_NAMESPACE

#endif // QABSTRACTFILEICONENGINE_P_H
