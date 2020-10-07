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

#ifndef QQUICKLISTVIEW_P_H
#define QQUICKLISTVIEW_P_H

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

QT_REQUIRE_CONFIG(quick_listview);

#include "qquickitemview_p.h"

#include <private/qtquickglobal_p.h>

QT_BEGIN_NAMESPACE

class QQuickListView;
class QQuickListViewPrivate;
class Q_QUICK_PRIVATE_EXPORT QQuickViewSection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString property READ property WRITE setProperty NOTIFY propertyChanged)
    Q_PROPERTY(SectionCriteria criteria READ criteria WRITE setCriteria NOTIFY criteriaChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(int labelPositioning READ labelPositioning WRITE setLabelPositioning NOTIFY labelPositioningChanged)
    QML_NAMED_ELEMENT(ViewSection)
public:
    QQuickViewSection(QQuickListView *parent=nullptr);

    QString property() const { return m_property; }
    void setProperty(const QString &);

    enum SectionCriteria { FullString, FirstCharacter };
    Q_ENUM(SectionCriteria)
    SectionCriteria criteria() const { return m_criteria; }
    void setCriteria(SectionCriteria);

    QQmlComponent *delegate() const { return m_delegate; }
    void setDelegate(QQmlComponent *delegate);

    QString sectionString(const QString &value);

    enum LabelPositioning { InlineLabels = 0x01, CurrentLabelAtStart = 0x02, NextLabelAtEnd = 0x04 };
    Q_ENUM(LabelPositioning)
    int labelPositioning() const { return m_labelPositioning; }
    void setLabelPositioning(int pos);

Q_SIGNALS:
    void sectionsChanged();
    void propertyChanged();
    void criteriaChanged();
    void delegateChanged();
    void labelPositioningChanged();

private:
    QString m_property;
    SectionCriteria m_criteria;
    QQmlComponent *m_delegate;
    int m_labelPositioning;
    QQuickListViewPrivate *m_view;
};


class QQmlInstanceModel;
class QQuickListViewAttached;
class Q_QUICK_PRIVATE_EXPORT QQuickListView : public QQuickItemView
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickListView)

    Q_PROPERTY(qreal highlightMoveVelocity READ highlightMoveVelocity WRITE setHighlightMoveVelocity NOTIFY highlightMoveVelocityChanged)
    Q_PROPERTY(qreal highlightResizeVelocity READ highlightResizeVelocity WRITE setHighlightResizeVelocity NOTIFY highlightResizeVelocityChanged)
    Q_PROPERTY(int highlightResizeDuration READ highlightResizeDuration WRITE setHighlightResizeDuration NOTIFY highlightResizeDurationChanged)

    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)

    Q_PROPERTY(QQuickViewSection *section READ sectionCriteria CONSTANT)
    Q_PROPERTY(QString currentSection READ currentSection NOTIFY currentSectionChanged)

    Q_PROPERTY(SnapMode snapMode READ snapMode WRITE setSnapMode NOTIFY snapModeChanged)

    Q_PROPERTY(HeaderPositioning headerPositioning READ headerPositioning WRITE setHeaderPositioning NOTIFY headerPositioningChanged REVISION 4)
    Q_PROPERTY(FooterPositioning footerPositioning READ footerPositioning WRITE setFooterPositioning NOTIFY footerPositioningChanged REVISION 4)

    Q_CLASSINFO("DefaultProperty", "data")
    QML_NAMED_ELEMENT(ListView)
    QML_ATTACHED(QQuickListViewAttached)

public:
    QQuickListView(QQuickItem *parent=nullptr);
    ~QQuickListView();

    qreal spacing() const;
    void setSpacing(qreal spacing);

    enum Orientation { Horizontal = Qt::Horizontal, Vertical = Qt::Vertical };
    Q_ENUM(Orientation)
    Orientation orientation() const;
    void setOrientation(Orientation);

    QQuickViewSection *sectionCriteria();
    QString currentSection() const;

    void setHighlightFollowsCurrentItem(bool) override;

    qreal highlightMoveVelocity() const;
    void setHighlightMoveVelocity(qreal);

    qreal highlightResizeVelocity() const;
    void setHighlightResizeVelocity(qreal);

    int highlightResizeDuration() const;
    void setHighlightResizeDuration(int);

    void setHighlightMoveDuration(int) override;

    enum SnapMode { NoSnap, SnapToItem, SnapOneItem };
    Q_ENUM(SnapMode)
    SnapMode snapMode() const;
    void setSnapMode(SnapMode mode);

    enum HeaderPositioning { InlineHeader, OverlayHeader, PullBackHeader };
    Q_ENUM(HeaderPositioning)
    HeaderPositioning headerPositioning() const;
    void setHeaderPositioning(HeaderPositioning positioning);

    enum FooterPositioning { InlineFooter, OverlayFooter, PullBackFooter };
    Q_ENUM(FooterPositioning)
    FooterPositioning footerPositioning() const;
    void setFooterPositioning(FooterPositioning positioning);

    static QQuickListViewAttached *qmlAttachedProperties(QObject *);

    bool contains(const QPointF &point) const override;

public Q_SLOTS:
    void incrementCurrentIndex();
    void decrementCurrentIndex();

Q_SIGNALS:
    void spacingChanged();
    void orientationChanged();
    void currentSectionChanged();
    void highlightMoveVelocityChanged();
    void highlightResizeVelocityChanged();
    void highlightResizeDurationChanged();
    void snapModeChanged();
    Q_REVISION(4) void headerPositioningChanged();
    Q_REVISION(4) void footerPositioningChanged();

protected:
    void viewportMoved(Qt::Orientations orient) override;
    void keyPressEvent(QKeyEvent *) override;
    void geometryChanged(const QRectF &newGeometry,const QRectF &oldGeometry) override;
    void initItem(int index, QObject *item) override;
    qreal maxYExtent() const override;
    qreal maxXExtent() const override;
};

class QQuickListViewAttached : public QQuickItemViewAttached
{
    Q_OBJECT

public:
    QQuickListViewAttached(QObject *parent)
        : QQuickItemViewAttached(parent), m_sectionItem(nullptr) {}
    ~QQuickListViewAttached() {}

public:
    QPointer<QQuickItem> m_sectionItem;
};


QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickListView)
QML_DECLARE_TYPE(QQuickViewSection)

#endif // QQUICKLISTVIEW_P_H
