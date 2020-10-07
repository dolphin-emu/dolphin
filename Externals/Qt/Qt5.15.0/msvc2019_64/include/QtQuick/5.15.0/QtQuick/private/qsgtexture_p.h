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

#ifndef QSGTEXTURE_P_H
#define QSGTEXTURE_P_H

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

#include <QtQuick/private/qtquickglobal_p.h>
#include <private/qobject_p.h>
#include "qsgtexture.h"

QT_BEGIN_NAMESPACE

struct QSGSamplerDescription
{
    QSGTexture::Filtering filtering = QSGTexture::Nearest;
    QSGTexture::Filtering mipmapFiltering = QSGTexture::None;
    QSGTexture::WrapMode horizontalWrap = QSGTexture::ClampToEdge;
    QSGTexture::WrapMode verticalWrap = QSGTexture::ClampToEdge;
    QSGTexture::AnisotropyLevel anisotropylevel = QSGTexture::AnisotropyNone;

    static QSGSamplerDescription fromTexture(QSGTexture *t);
};

Q_DECLARE_TYPEINFO(QSGSamplerDescription, Q_MOVABLE_TYPE);

bool operator==(const QSGSamplerDescription &a, const QSGSamplerDescription &b) Q_DECL_NOTHROW;
bool operator!=(const QSGSamplerDescription &a, const QSGSamplerDescription &b) Q_DECL_NOTHROW;
uint qHash(const QSGSamplerDescription &s, uint seed = 0) Q_DECL_NOTHROW;

class Q_QUICK_PRIVATE_EXPORT QSGTexturePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QSGTexture)
public:
    QSGTexturePrivate();
    static QSGTexturePrivate *get(QSGTexture *t) { return t->d_func(); }
    void resetDirtySamplerOptions();
    bool hasDirtySamplerOptions() const;

    virtual QRhiTexture *rhiTexture() const;

    // ### Qt 6: these should be virtuals in the public class instead
    virtual int comparisonKey() const; // ### Qt 6: pure virtual
    virtual void updateRhiTexture(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates);

    QRhiResourceUpdateBatch *workResourceUpdateBatch = nullptr; // ### Qt 6: remove

    uint wrapChanged : 1;
    uint filteringChanged : 1;
    uint anisotropyChanged : 1;

    uint horizontalWrap : 2;
    uint verticalWrap : 2;
    uint mipmapMode : 2;
    uint filterMode : 2;
    uint anisotropyLevel: 3;
};

Q_QUICK_PRIVATE_EXPORT bool qsg_safeguard_texture(QSGTexture *);

QT_END_NAMESPACE

#endif // QSGTEXTURE_P_H
