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

#ifndef QQUICKWIDGET_H
#define QQUICKWIDGET_H

#include <QtWidgets/qwidget.h>
#include <QtQuick/qquickwindow.h>
#include <QtCore/qurl.h>
#include <QtQuickWidgets/qtquickwidgetsglobal.h>
#include <QtGui/qimage.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;
class QQmlContext;
class QQmlError;
class QQuickItem;
class QQmlComponent;

class QQuickWidgetPrivate;
class Q_QUICKWIDGETS_EXPORT QQuickWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl source READ source WRITE setSource DESIGNABLE true)

public:
    explicit QQuickWidget(QWidget *parent = nullptr);
    QQuickWidget(QQmlEngine* engine, QWidget *parent);
    explicit QQuickWidget(const QUrl &source, QWidget *parent = nullptr);
    ~QQuickWidget() override;

    QUrl source() const;

    QQmlEngine* engine() const;
    QQmlContext* rootContext() const;

    QQuickItem *rootObject() const;

    enum ResizeMode { SizeViewToRootObject, SizeRootObjectToView };
    Q_ENUM(ResizeMode)
    ResizeMode resizeMode() const;
    void setResizeMode(ResizeMode);

    enum Status { Null, Ready, Loading, Error };
    Q_ENUM(Status)
    Status status() const;

    QList<QQmlError> errors() const;

    QSize sizeHint() const override;
    QSize initialSize() const;

    void setFormat(const QSurfaceFormat &format);
    QSurfaceFormat format() const;

    QImage grabFramebuffer() const;

    void setClearColor(const QColor &color);

    QQuickWindow *quickWindow() const;

public Q_SLOTS:
    void setSource(const QUrl&);
    void setContent(const QUrl& url, QQmlComponent *component, QObject *item);

Q_SIGNALS:
    void statusChanged(QQuickWidget::Status);
    void sceneGraphError(QQuickWindow::SceneGraphError error, const QString &message);

private Q_SLOTS:
    // ### Qt 6: make these truly private slots through Q_PRIVATE_SLOT
    void continueExecute();
    void createFramebufferObject();
    void destroyFramebufferObject();
    void triggerUpdate();

protected:
    void resizeEvent(QResizeEvent *) override;
    void timerEvent(QTimerEvent*) override;

    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

    void focusInEvent(QFocusEvent * event) override;
    void focusOutEvent(QFocusEvent * event) override;

#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *) override;
#endif

#if QT_CONFIG(quick_draganddrop)
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
#endif

    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    Q_DISABLE_COPY(QQuickWidget)
    Q_DECLARE_PRIVATE(QQuickWidget)
};

QT_END_NAMESPACE

#endif // QQuickWidget_H
