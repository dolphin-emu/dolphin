/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QSIMPLEDRAG_P_H
#define QSIMPLEDRAG_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include <qpa/qplatformdrag.h>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtGui/QWindow>

QT_REQUIRE_CONFIG(draganddrop);

QT_BEGIN_NAMESPACE

class QMouseEvent;
class QEventLoop;
class QDropData;
class QShapedPixmapWindow;
class QScreen;

class Q_GUI_EXPORT QBasicDrag : public QPlatformDrag, public QObject
{
public:
    ~QBasicDrag();

    virtual Qt::DropAction drag(QDrag *drag) override;
    void cancelDrag() override;

    virtual bool eventFilter(QObject *o, QEvent *e) override;

protected:
    QBasicDrag();

    virtual void startDrag();
    virtual void cancel();
    virtual void move(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) = 0;
    virtual void drop(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) = 0;
    virtual void endDrag();


    void moveShapedPixmapWindow(const QPoint &deviceIndependentPosition);
    QShapedPixmapWindow *shapedPixmapWindow() const { return m_drag_icon_window; }
    void recreateShapedPixmapWindow(QScreen *screen, const QPoint &pos);
    void updateCursor(Qt::DropAction action);

    bool canDrop() const { return m_can_drop; }
    void setCanDrop(bool c) { m_can_drop = c; }

    bool useCompositing() const { return m_useCompositing; }
    void setUseCompositing(bool on) { m_useCompositing = on; }

    void setScreen(QScreen *screen) { m_screen = screen; }

    Qt::DropAction executedDropAction() const { return m_executed_drop_action; }
    void  setExecutedDropAction(Qt::DropAction da) { m_executed_drop_action = da; }

    QDrag *drag() const { return m_drag; }

protected:
    QWindow *m_sourceWindow = nullptr;
    QPointer<QWindow> m_windowUnderCursor = nullptr;

private:
    void enableEventFilter();
    void disableEventFilter();
    void restoreCursor();
    void exitDndEventLoop();

#ifndef QT_NO_CURSOR
    bool m_dndHasSetOverrideCursor = false;
#endif
    QEventLoop *m_eventLoop = nullptr;
    Qt::DropAction m_executed_drop_action = Qt::IgnoreAction;
    bool m_can_drop = false;
    QDrag *m_drag = nullptr;
    QShapedPixmapWindow *m_drag_icon_window = nullptr;
    bool m_useCompositing = true;
    QScreen *m_screen = nullptr;
};

class Q_GUI_EXPORT QSimpleDrag : public QBasicDrag
{
public:
    QSimpleDrag();

protected:
    virtual void startDrag() override;
    virtual void cancel() override;
    virtual void move(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) override;
    virtual void drop(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) override;
};

QT_END_NAMESPACE

#endif
