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

#include <private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_shadereffect);

#include "qqmlparserstatus.h"

#include <QtQuick/qtquickglobal.h>
#include <QtGui/qcolor.h>
#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qvector.h>
#include <QtCore/qbytearray.h>
#include <QtQml/qqml.h>

#ifndef QQUICKSHADEREFFECTMESH_P_H
#define QQUICKSHADEREFFECTMESH_P_H

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

QT_BEGIN_NAMESPACE

Q_QUICK_PRIVATE_EXPORT const char *qtPositionAttributeName();
Q_QUICK_PRIVATE_EXPORT const char *qtTexCoordAttributeName();

class QSGGeometry;
class QRectF;

class Q_QUICK_PRIVATE_EXPORT QQuickShaderEffectMesh : public QObject
{
    Q_OBJECT

    QML_NAMED_ELEMENT(ShaderEffectMesh)
    QML_UNCREATABLE("Cannot create instance of abstract class ShaderEffectMesh.")

public:
    QQuickShaderEffectMesh(QObject *parent = nullptr);
    virtual bool validateAttributes(const QVector<QByteArray> &attributes, int *posIndex) = 0;
    // If 'geometry' != 0, 'attrCount' is the same as last time the function was called.
    virtual QSGGeometry *updateGeometry(QSGGeometry *geometry, int attrCount, int posIndex,
                                        const QRectF &srcRect, const QRectF &rect) = 0;
    // If updateGeometry() fails, the reason should appear in the log.
    virtual QString log() const { return QString(); }

Q_SIGNALS:
    // Emitted when the geometry needs to be updated.
    void geometryChanged();

protected:
    QQuickShaderEffectMesh(QObjectPrivate &dd, QObject *parent = nullptr);
};

class Q_QUICK_PRIVATE_EXPORT QQuickGridMesh : public QQuickShaderEffectMesh
{
    Q_OBJECT
    Q_PROPERTY(QSize resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    QML_NAMED_ELEMENT(GridMesh)
public:
    QQuickGridMesh(QObject *parent = nullptr);
    bool validateAttributes(const QVector<QByteArray> &attributes, int *posIndex) override;
    QSGGeometry *updateGeometry(QSGGeometry *geometry, int attrCount, int posIndex,
                                const QRectF &srcRect, const QRectF &rect) override;
    QString log() const  override { return m_log; }

    void setResolution(const QSize &res);
    QSize resolution() const;

Q_SIGNALS:
    void resolutionChanged();

private:
    QSize m_resolution;
    QString m_log;
};

class QQuickScaleGrid;
class QQuickBorderImageMesh : public QQuickShaderEffectMesh
{
    Q_OBJECT

    Q_PROPERTY(QQuickScaleGrid *border READ border CONSTANT)
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(TileMode horizontalTileMode READ horizontalTileMode WRITE setHorizontalTileMode NOTIFY horizontalTileModeChanged)
    Q_PROPERTY(TileMode verticalTileMode READ verticalTileMode WRITE setVerticalTileMode NOTIFY verticalTileModeChanged)

    QML_NAMED_ELEMENT(BorderImageMesh)
    QML_ADDED_IN_MINOR_VERSION(8)

public:
    QQuickBorderImageMesh(QObject *parent = nullptr);

    bool validateAttributes(const QVector<QByteArray> &attributes, int *posIndex) override;
    QSGGeometry *updateGeometry(QSGGeometry *geometry, int attrCount, int posIndex,
                                const QRectF &srcRect, const QRectF &rect) override;

    QQuickScaleGrid *border() const;

    enum TileMode { Stretch = Qt::StretchTile, Repeat = Qt::RepeatTile, Round = Qt::RoundTile };
    Q_ENUM(TileMode)

    QSize size() const;
    void setSize(const QSize &size);

    TileMode horizontalTileMode() const;
    void setHorizontalTileMode(TileMode);

    TileMode verticalTileMode() const;
    void setVerticalTileMode(TileMode);

Q_SIGNALS:
    void sizeChanged();
    void horizontalTileModeChanged();
    void verticalTileModeChanged();

private:
    QQuickScaleGrid *m_border;
    QSize m_size;
    TileMode m_horizontalTileMode;
    TileMode m_verticalTileMode;
};

inline QColor qt_premultiply_color(const QColor &c)
{
    return QColor::fromRgbF(c.redF() * c.alphaF(), c.greenF() * c.alphaF(), c.blueF() * c.alphaF(), c.alphaF());
}


QT_END_NAMESPACE

#endif // QQUICKSHADEREFFECTMESH_P_H
