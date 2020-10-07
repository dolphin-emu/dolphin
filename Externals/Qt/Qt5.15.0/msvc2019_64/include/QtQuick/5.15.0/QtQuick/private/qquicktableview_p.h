/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QQUICKTABLEVIEW_P_H
#define QQUICKTABLEVIEW_P_H

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
QT_REQUIRE_CONFIG(quick_tableview);

#include <QtCore/qpointer.h>
#include <QtQuick/private/qtquickglobal_p.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQml/private/qqmlnullablevalue_p.h>

QT_BEGIN_NAMESPACE

class QQuickTableViewAttached;
class QQuickTableViewPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickTableView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns NOTIFY columnsChanged)
    Q_PROPERTY(qreal rowSpacing READ rowSpacing WRITE setRowSpacing NOTIFY rowSpacingChanged)
    Q_PROPERTY(qreal columnSpacing READ columnSpacing WRITE setColumnSpacing NOTIFY columnSpacingChanged)
    Q_PROPERTY(QJSValue rowHeightProvider READ rowHeightProvider WRITE setRowHeightProvider NOTIFY rowHeightProviderChanged)
    Q_PROPERTY(QJSValue columnWidthProvider READ columnWidthProvider WRITE setColumnWidthProvider NOTIFY columnWidthProviderChanged)
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(bool reuseItems READ reuseItems WRITE setReuseItems NOTIFY reuseItemsChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth WRITE setContentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight WRITE setContentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(QQuickTableView *syncView READ syncView WRITE setSyncView NOTIFY syncViewChanged REVISION 14)
    Q_PROPERTY(Qt::Orientations syncDirection READ syncDirection WRITE setSyncDirection NOTIFY syncDirectionChanged REVISION 14)

    QML_NAMED_ELEMENT(TableView)
    QML_ADDED_IN_MINOR_VERSION(12)
    QML_ATTACHED(QQuickTableViewAttached)

public:
    QQuickTableView(QQuickItem *parent = nullptr);
    ~QQuickTableView() override;
    int rows() const;
    int columns() const;

    qreal rowSpacing() const;
    void setRowSpacing(qreal spacing);

    qreal columnSpacing() const;
    void setColumnSpacing(qreal spacing);

    QJSValue rowHeightProvider() const;
    void setRowHeightProvider(const QJSValue &provider);

    QJSValue columnWidthProvider() const;
    void setColumnWidthProvider(const QJSValue &provider);

    QVariant model() const;
    void setModel(const QVariant &newModel);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    bool reuseItems() const;
    void setReuseItems(bool reuseItems);

    void setContentWidth(qreal width);
    void setContentHeight(qreal height);

    QQuickTableView *syncView() const;
    void setSyncView(QQuickTableView *view);

    Qt::Orientations syncDirection() const;
    void setSyncDirection(Qt::Orientations direction);

    Q_INVOKABLE void forceLayout();

    static QQuickTableViewAttached *qmlAttachedProperties(QObject *);

Q_SIGNALS:
    void rowsChanged();
    void columnsChanged();
    void rowSpacingChanged();
    void columnSpacingChanged();
    void rowHeightProviderChanged();
    void columnWidthProviderChanged();
    void modelChanged();
    void delegateChanged();
    void reuseItemsChanged();
    Q_REVISION(14) void syncViewChanged();
    Q_REVISION(14) void syncDirectionChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void viewportMoved(Qt::Orientations orientation) override;
    void componentComplete() override;

protected:
    QQuickTableView(QQuickTableViewPrivate &dd, QQuickItem *parent);

private:
    Q_DISABLE_COPY(QQuickTableView)
    Q_DECLARE_PRIVATE(QQuickTableView)

    qreal minXExtent() const override;
    qreal maxXExtent() const override;
    qreal minYExtent() const override;
    qreal maxYExtent() const override;

    Q_PRIVATE_SLOT(d_func(), void _q_componentFinalized())
};

class Q_QUICK_PRIVATE_EXPORT QQuickTableViewAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTableView *view READ view NOTIFY viewChanged)

public:
    QQuickTableViewAttached(QObject *parent)
        : QObject(parent) {}

    QQuickTableView *view() const { return m_view; }
    void setView(QQuickTableView *newTableView) {
        if (newTableView == m_view)
            return;
        m_view = newTableView;
        Q_EMIT viewChanged();
    }

Q_SIGNALS:
    void viewChanged();
    void pooled();
    void reused();

private:
    QPointer<QQuickTableView> m_view;

    friend class QQuickTableViewPrivate;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTableView)

#endif // QQUICKTABLEVIEW_P_H
