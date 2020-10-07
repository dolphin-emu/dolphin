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

#ifndef QQUICKSPRITESEQUENCE_P_H
#define QQUICKSPRITESEQUENCE_P_H

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

#include <private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_sprite);

#include <QtQuick/QQuickItem>

QT_BEGIN_NAMESPACE

class QSGContext;
class QQuickSprite;
class QQuickSpriteEngine;
class QQuickSpriteSequencePrivate;
class QSGSpriteNode;
class Q_AUTOTEST_EXPORT QQuickSpriteSequence : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(bool interpolate READ interpolate WRITE setInterpolate NOTIFY interpolateChanged)
    Q_PROPERTY(QString goalSprite READ goalSprite WRITE setGoalSprite NOTIFY goalSpriteChanged)
    Q_PROPERTY(QString currentSprite READ currentSprite NOTIFY currentSpriteChanged)
    //###try to share similar spriteEngines for less overhead?
    Q_PROPERTY(QQmlListProperty<QQuickSprite> sprites READ sprites)
    Q_CLASSINFO("DefaultProperty", "sprites")
    QML_NAMED_ELEMENT(SpriteSequence)

public:
    explicit QQuickSpriteSequence(QQuickItem *parent = nullptr);

    QQmlListProperty<QQuickSprite> sprites();

    bool running() const;
    bool interpolate() const;
    QString goalSprite() const;
    QString currentSprite() const;

Q_SIGNALS:

    void runningChanged(bool arg);
    void interpolateChanged(bool arg);
    void goalSpriteChanged(const QString &arg);
    void currentSpriteChanged(const QString &arg);

public Q_SLOTS:

    void jumpTo(const QString &sprite);
    void setGoalSprite(const QString &sprite);
    void setRunning(bool arg);
    void setInterpolate(bool arg);

private Q_SLOTS:
    void createEngine();

protected:
    void reset();
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
private:
    void prepareNextFrame(QSGSpriteNode *node);
    QSGSpriteNode* initNode();


private:
    Q_DISABLE_COPY(QQuickSpriteSequence)
    Q_DECLARE_PRIVATE(QQuickSpriteSequence)
};

QT_END_NAMESPACE

#endif // QQUICKSPRITESEQUENCE_P_H
